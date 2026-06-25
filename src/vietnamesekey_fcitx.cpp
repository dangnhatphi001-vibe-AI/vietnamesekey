#include <fcitx/addonfactory.h>
#include <fcitx/addoninstance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <string>
#include <fcitx/inputpanel.h>
#include <fcitx/text.h>

#include "shadow_engine.h"

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

        // Pass through modifier combos untouched
        if ((states & fcitx::KeyState::Ctrl) ||
            (states & fcitx::KeyState::Alt)  ||
            (states & fcitx::KeyState::Super)) {
            flush(ic);
            return;
        }

        // Escape: discard composition
        if (sym == FcitxKey_Escape) {
            if (engine_.has_composition()) {
                wipe(ic);
                ke.filterAndAccept();
            }
            return;
        }

        // Enter: commit as-is
        if (sym == FcitxKey_Return || sym == FcitxKey_KP_Enter) {
            flush(ic);
            return;
        }

        // Backspace inside composition
        if (sym == FcitxKey_BackSpace) {
            if (engine_.has_composition()) {
                char32_t obuf[32];
                size_t   olen = 0;
                engine_.process_key(8, mode, obuf, olen);
                show(ic, obuf, olen);
                ke.filterAndAccept();
            }
            return;
        }

        // Non-printable ASCII: flush and pass through
        if (sym < 32 || sym > 126) {
            flush(ic);
            return;
        }

        // Word break: flush and let the key pass through to app
        if (ShadowEngine::is_word_break(static_cast<char32_t>(sym), mode)) {
            flush(ic);
            return;
        }

        // ── Main composition path ──
        char32_t obuf[32];
        size_t   olen = 0;
        const bool consumed =
            engine_.process_key(static_cast<char32_t>(sym), mode, obuf, olen);

        if (consumed) {
            show(ic, obuf, olen);
            ke.filterAndAccept();
        } else {
            // Engine says "not part of Vietnamese" — commit whatever we had
            if (olen > 0) {
                // Engine returned final output
                commit_buf(ic, obuf, olen);
            } else {
                flush(ic);
            }
        }
    }

    void activate(const fcitx::InputMethodEntry&, fcitx::InputContextEvent&) override {}

    void reset(const fcitx::InputMethodEntry&,
               fcitx::InputContextEvent& ev) override {
        flush(ev.inputContext());
    }

    void deactivate(const fcitx::InputMethodEntry& entry,
                    fcitx::InputContextEvent&       ev) override {
        flush(ev.inputContext());
        InputMethodEngineV2::deactivate(entry, ev);
    }

    std::string subModeLabelImpl(const fcitx::InputMethodEntry&, fcitx::InputContext&) override {
        return "";
    }

private:
    fcitx::Instance* instance_;
    ShadowEngine     engine_;
    size_t           m_surr = 0;   // chars committed via SurroundingText (for delete)
    char             m_utf8[256];  // cached UTF-8 buffer (avoid repeated encoding)
    size_t           m_utf8n = 0;  // cached length

    // ── Capability checks (inlined, zero-cost) ──

    static bool cap_surrounding(fcitx::InputContext* ic) {
        return ic->capabilityFlags().test(fcitx::CapabilityFlag::SurroundingText);
    }

    static bool cap_preedit(fcitx::InputContext* ic) {
        return ic->capabilityFlags().test(fcitx::CapabilityFlag::Preedit);
    }

    // ── Encode once, use twice ──

    void encode(const char32_t* buf, size_t len) {
        m_utf8n = (len > 0)
            ? encode_utf8(buf, len, m_utf8, sizeof(m_utf8) - 1)
            : 0;
        m_utf8[m_utf8n] = '\0';
    }

    // ── Core display: dual-channel preedit ──
    //
    // SurroundingText-capable apps  → deleteSurroundingText + commitString (instant)
    // Other apps                    → setClientPreedit (inline) + setPreedit (floating box)
    //
    // Both channels are updated in a single call to minimise round-trips.

    void show(fcitx::InputContext* ic, const char32_t* obuf, size_t olen) {
        encode(obuf, olen);

        if (cap_surrounding(ic)) {
            // ── Fast path: direct text manipulation ──
            if (m_surr > 0) {
                ic->deleteSurroundingText(-static_cast<int>(m_surr), m_surr);
            }
            if (m_utf8n > 0) {
                ic->commitString(std::string(m_utf8, m_utf8n));
            }
            m_surr = olen;

            // Keep panels clean
            ic->inputPanel().setPreedit(fcitx::Text());
            ic->inputPanel().setClientPreedit(fcitx::Text());
            ic->updatePreedit();
        } else {
            // ── Preedit path: dual-channel for speed + floating box ──
            m_surr = 0;
            std::string s(m_utf8, m_utf8n);

            // 1) Client preedit → inline in the text field (zero-delay)
            fcitx::Text client;
            if (m_utf8n > 0) {
                client.append(s);
                client.setCursor(static_cast<int>(m_utf8n));
            }
            ic->inputPanel().setClientPreedit(client);

            // 2) Panel preedit → floating box (plain text, no highlight/underline)
            fcitx::Text panel;
            if (m_utf8n > 0) {
                panel.append(s);
            }
            ic->inputPanel().setPreedit(panel);

            // Single updatePreedit pushes both channels at once
            ic->updatePreedit();

            // Show the floating panel
            ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
        }
    }

    // ── Commit final text and clear everything ──

    void commit_buf(fcitx::InputContext* ic, const char32_t* obuf, size_t olen) {
        encode(obuf, olen);
        if (m_utf8n > 0) {
            if (!cap_surrounding(ic)) {
                // Must commit the preedit text to the application
                ic->commitString(std::string(m_utf8, m_utf8n));
            }
            // For SurroundingText apps the text is already committed in show()
        }
        wipe(ic);
    }

    // ── Flush: commit current composition and reset ──

    void flush(fcitx::InputContext* ic) {
        if (engine_.has_composition()) {
            if (!cap_surrounding(ic)) {
                // Preedit mode: need to commit the text
                char32_t obuf[32];
                size_t olen = 0;
                engine_.get_composition(obuf, olen);
                if (olen > 0) {
                    encode(obuf, olen);
                    ic->commitString(std::string(m_utf8, m_utf8n));
                }
            }
            // SurroundingText mode: text already committed char by char
            wipe(ic);
        }
    }

    // ── Wipe all state and clear UI ──

    void wipe(fcitx::InputContext* ic) {
        engine_.reset();
        m_surr = 0;
        m_utf8n = 0;

        ic->inputPanel().setPreedit(fcitx::Text());
        ic->inputPanel().setClientPreedit(fcitx::Text());
        ic->updatePreedit();
        ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    }
};

class VietnameseKeyFactory final : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance* create(fcitx::AddonManager* manager) override {
        return new VietnameseKeyEngine(manager->instance());
    }
};

FCITX_ADDON_FACTORY(VietnameseKeyFactory)