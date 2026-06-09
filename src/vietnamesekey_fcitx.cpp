// vietnamesekey_fcitx.cpp
// Fcitx5 plugin wrapper for the ShadowEngine Vietnamese composition engine.
// Implements full lifecycle: keyEvent, reset (focus-in reset), deactivate (focus-out commit).

#include <fcitx/addonfactory.h>
#include <fcitx/addoninstance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/text.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <string>

#include "shadow_engine.h"

// ============================================================
//  UTF-32 → UTF-8 encoder
//  Hand-crafted bit-shifting on a stack array; zero allocation.
//  Returns number of bytes written into dst[].
// ============================================================
static inline size_t encode_utf8(const char32_t* __restrict__ src,
                                  size_t                        src_len,
                                  char*       __restrict__     dst,
                                  size_t                        dst_cap) {
    size_t n = 0;
    for (size_t i = 0; i < src_len; ++i) {
        const char32_t cp = src[i];
        if (cp <= 0x7Fu) {
            if (n + 1 > dst_cap) break;
            dst[n++] = static_cast<char>(cp);
        } else if (cp <= 0x7FFu) {
            if (n + 2 > dst_cap) break;
            dst[n++] = static_cast<char>(0xC0u | ((cp >>  6) & 0x1Fu));
            dst[n++] = static_cast<char>(0x80u |  (cp        & 0x3Fu));
        } else if (cp <= 0xFFFFu) {
            if (n + 3 > dst_cap) break;
            dst[n++] = static_cast<char>(0xE0u | ((cp >> 12) & 0x0Fu));
            dst[n++] = static_cast<char>(0x80u | ((cp >>  6) & 0x3Fu));
            dst[n++] = static_cast<char>(0x80u |  (cp        & 0x3Fu));
        } else if (cp <= 0x10FFFFu) {
            if (n + 4 > dst_cap) break;
            dst[n++] = static_cast<char>(0xF0u | ((cp >> 18) & 0x07u));
            dst[n++] = static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
            dst[n++] = static_cast<char>(0x80u | ((cp >>  6) & 0x3Fu));
            dst[n++] = static_cast<char>(0x80u |  (cp        & 0x3Fu));
        }
    }
    return n;
}

// ============================================================
//  VietnameseKeyEngine
// ============================================================
class VietnameseKeyEngine final : public fcitx::InputMethodEngineV2 {
public:
    explicit VietnameseKeyEngine(fcitx::Instance* instance)
        : instance_(instance) {}

    // --------------------------------------------------------
    //  keyEvent — hot path, called on every key-press.
    // --------------------------------------------------------
    void keyEvent(const fcitx::InputMethodEntry& entry,
                  fcitx::KeyEvent&               ke) override {

        // Key release events carry no composition meaning.
        if (ke.isRelease()) return;

        fcitx::InputContext* ic = ke.inputContext();
        if (!ic) return;

        // Detect input mode from the registered IME unique name.
        const uint8_t mode =
            (entry.uniqueName() == "vietnamesekey-vni") ? 1u : 0u;

        const FcitxKeySym      sym    = ke.key().sym();
        const fcitx::KeyStates states = ke.key().states();

        // ---- Modifier shortcuts (Ctrl/Alt/Super) → commit + pass through.
        const bool has_ctrl_alt_super =
            (states & fcitx::KeyState::Ctrl)  ||
            (states & fcitx::KeyState::Alt)   ||
            (states & fcitx::KeyState::Super);

        if (has_ctrl_alt_super) {
            commit_pending(ic);
            return;   // do NOT call filterAndAccept(); let the shortcut pass
        }

        // ---- Escape:
        //   With composition → discard silently and SWALLOW Escape (filterAndAccept).
        //   Without composition → pass Escape through to the application.
        if (sym == FcitxKey_Escape) {
            if (engine_.has_composition()) {
                engine_.reset();
                clear_preedit(ic);
                ke.filterAndAccept();   // swallow: app should not also react to Escape
            }
            return;
        }

        // ---- Enter / Return: commit then let Enter reach the application.
        if (sym == FcitxKey_Return || sym == FcitxKey_KP_Enter) {
            commit_pending(ic);
            return;   // pass Enter through
        }

        // ---- BackSpace handled specially: try to erase one composition char.
        if (sym == FcitxKey_BackSpace) {
            if (engine_.has_composition()) {
                char32_t obuf[32];
                size_t   olen = 0;
                engine_.process_key(8, mode, obuf, olen);
                if (engine_.has_composition()) {
                    update_preedit(ic, obuf, olen);
                } else {
                    clear_preedit(ic);
                }
                ke.filterAndAccept();   // eat the BackSpace; preedit updated above
                return;
            }
            return;   // no composition: let BackSpace propagate normally
        }

        // ---- Non-ASCII / function keys: commit + pass through.
        if (sym < 32 || sym > 126) {
            commit_pending(ic);
            return;
        }

        // ---- Space and punctuation: commit + pass through.
        if (ShadowEngine::is_word_break(static_cast<char32_t>(sym), mode)) {
            commit_pending(ic);
            return;   // pass the character through
        }

        // ---- Feed the key to the composition engine.
        char32_t obuf[32];
        size_t   olen = 0;
        const bool handled =
            engine_.process_key(static_cast<char32_t>(sym), mode, obuf, olen);

        if (handled) {
            update_preedit(ic, obuf, olen);
            ke.filterAndAccept();
        } else {
            if (olen > 0) {
                commit_utf32(ic, obuf, olen);
                clear_preedit(ic);
            } else {
                commit_pending(ic);
            }
        }
    }

    // --------------------------------------------------------
    //  reset() — called by Fcitx5 when the IC needs a clean slate
    //  while still focused (e.g. programmatic reset from another addon).
    // --------------------------------------------------------
    void reset(const fcitx::InputMethodEntry&,
               fcitx::InputContextEvent& ev) override {
        fcitx::InputContext* ic = ev.inputContext();
        engine_.reset();
        if (ic) clear_preedit(ic);
    }

    // --------------------------------------------------------
    //  deactivate() — called when the user switches away from this IM,
    //  changes focus to another window, or clicks the cursor elsewhere.
    //  Commit any pending composition so text is not lost.
    // --------------------------------------------------------
    void deactivate(const fcitx::InputMethodEntry& entry,
                    fcitx::InputContextEvent&       ev) override {
        fcitx::InputContext* ic = ev.inputContext();
        if (ic && engine_.has_composition()) {
            char32_t obuf[32];
            size_t   olen = 0;
            engine_.get_composition(obuf, olen);
            commit_utf32(ic, obuf, olen);
            engine_.reset();
            clear_preedit(ic);
        }
        // Also call the base implementation for any housekeeping it does.
        InputMethodEngineV2::deactivate(entry, ev);
    }

private:
    fcitx::Instance* instance_;
    ShadowEngine     engine_;

    // ---- Commit the pending composition (if any) without filtering. ----
    void commit_pending(fcitx::InputContext* ic) {
        if (!engine_.has_composition()) return;
        char32_t obuf[32];
        size_t   olen = 0;
        engine_.get_composition(obuf, olen);
        commit_utf32(ic, obuf, olen);
        engine_.reset();
        clear_preedit(ic);
    }

    // ---- Encode obuf as UTF-8 and commit to the application. ----------
    void commit_utf32(fcitx::InputContext* ic,
                      const char32_t* obuf, size_t olen) {
        if (olen == 0) return;
        // UTF-32 Vietnamese syllable → at most 3 UTF-8 bytes each, plus null.
        char utf8[128];
        const size_t nb = encode_utf8(obuf, olen, utf8, sizeof(utf8) - 1);
        utf8[nb] = '\0';
        // commitString requires const std::string&; construct on stack via SSO.
        ic->commitString(std::string(utf8, nb));
    }

    // ---- Set the preedit to the current obuf. --------------------------
    void update_preedit(fcitx::InputContext* ic,
                        const char32_t* obuf, size_t olen) {
        char utf8[128];
        const size_t nb = encode_utf8(obuf, olen, utf8, sizeof(utf8) - 1);
        utf8[nb] = '\0';
        fcitx::Text txt(std::string(utf8, nb));
        txt.setCursor(static_cast<int>(nb));   // cursor at end of preedit
        ic->inputPanel().setPreedit(txt);
        ic->updatePreedit();
    }

    // ---- Clear preedit panel. ------------------------------------------
    void clear_preedit(fcitx::InputContext* ic) {
        ic->inputPanel().setPreedit(fcitx::Text());
        ic->updatePreedit();
    }
};

// ============================================================
//  Addon factory — the single export required by Fcitx5.
// ============================================================
class VietnameseKeyFactory final : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance* create(fcitx::AddonManager* manager) override {
        return new VietnameseKeyEngine(manager->instance());
    }
};

FCITX_ADDON_FACTORY(VietnameseKeyFactory)
