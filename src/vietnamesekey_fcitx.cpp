// vietnamesekey_fcitx.cpp
// Fcitx5 plugin wrapper for the ShadowEngine Vietnamese composition engine.

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
// ============================================================
static inline size_t encode_utf8(const char32_t* __restrict__ src,
                                  size_t                        src_len,
                                  char* __restrict__     dst,
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

    void keyEvent(const fcitx::InputMethodEntry& entry,
                  fcitx::KeyEvent&               ke) override {

        if (ke.isRelease()) return;

        fcitx::InputContext* ic = ke.inputContext();
        if (!ic) return;

        const uint8_t mode =
            (entry.uniqueName() == "vietnamesekey-vni") ? 1u : 0u;

        const FcitxKeySym      sym    = ke.key().sym();
        const fcitx::KeyStates states = ke.key().states();

        const bool has_ctrl_alt_super =
            (states & fcitx::KeyState::Ctrl)  ||
            (states & fcitx::KeyState::Alt)   ||
            (states & fcitx::KeyState::Super);

        if (has_ctrl_alt_super) {
            commit_pending(ic);
            return;
        }

        if (sym == FcitxKey_Escape) {
            if (engine_.has_composition()) {
                engine_.reset();
                clear_preedit(ic);
                ke.filterAndAccept();
            }
            return;
        }

        if (sym == FcitxKey_Return || sym == FcitxKey_KP_Enter) {
            commit_pending(ic);
            return;
        }

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
                ke.filterAndAccept();
                return;
            }
            return;
        }

        if (sym < 32 || sym > 126) {
            commit_pending(ic);
            return;
        }

        if (ShadowEngine::is_word_break(static_cast<char32_t>(sym), mode)) {
            commit_pending(ic);
            return;
        }

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

    void activate(const fcitx::InputMethodEntry&, fcitx::InputContextEvent&) override {}

    void reset(const fcitx::InputMethodEntry&,
               fcitx::InputContextEvent& ev) override {
        fcitx::InputContext* ic = ev.inputContext();
        engine_.reset();
        if (ic) clear_preedit(ic);
    }

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
        InputMethodEngineV2::deactivate(entry, ev);
    }

    std::string subModeLabelImpl(const fcitx::InputMethodEntry&, fcitx::InputContext&) override {
        return "";
    }

private:
    fcitx::Instance* instance_;
    ShadowEngine     engine_;

    void commit_pending(fcitx::InputContext* ic) {
        if (!engine_.has_composition()) return;
        char32_t obuf[32];
        size_t   olen = 0;
        engine_.get_composition(obuf, olen);
        commit_utf32(ic, obuf, olen);
        engine_.reset();
        clear_preedit(ic);
    }

    void commit_utf32(fcitx::InputContext* ic,
                      const char32_t* obuf, size_t olen) {
        if (olen == 0) return;
        char utf8[256];
        const size_t nb = encode_utf8(obuf, olen, utf8, sizeof(utf8) - 1);
        utf8[nb] = '\0';
        ic->commitString(std::string(utf8, nb));
    }

    // ============================================================
    // FIXED PREEDIT LOGIC: Underline flag + UI Update explicitly
    // ============================================================
    void update_preedit(fcitx::InputContext* ic,
                        const char32_t* obuf, size_t olen) {
        char utf8[256];
        const size_t nb = encode_utf8(obuf, olen, utf8, sizeof(utf8) - 1);
        utf8[nb] = '\0';

        fcitx::Text txt;
        // Bắt buộc phải có format Underline để GTK/Qt/Chrome hiển thị chữ đang gõ
        txt.append(std::string(utf8, nb), fcitx::TextFormatFlag::Underline);

        ic->inputPanel().setPreedit(txt);
        ic->updatePreedit();
        // Ép Fcitx5 vẽ lại bảng InputPanel
        ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    }

    void clear_preedit(fcitx::InputContext* ic) {
        ic->inputPanel().setPreedit(fcitx::Text());
        ic->updatePreedit();
        ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    }
};

// ============================================================
//  Addon factory
// ============================================================
class VietnameseKeyFactory final : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance* create(fcitx::AddonManager* manager) override {
        return new VietnameseKeyEngine(manager->instance());
    }
};

FCITX_ADDON_FACTORY(VietnameseKeyFactory)