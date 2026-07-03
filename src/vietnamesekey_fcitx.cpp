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
#include <vector>
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

    std::vector<fcitx::InputMethodEntry> listInputMethods() override {
        std::vector<fcitx::InputMethodEntry> entries;
        entries.emplace_back("vietnamesekey-telex",
                             "VietnameseKey-Telex",
                             "vi",
                             "vietnamesekey");
        entries.back()
            .setIcon("vietnamesekey")
            .setLabel("vi")
            .setConfigurable(false);

        entries.emplace_back("vietnamesekey-vni",
                             "VietnameseKey-VNI",
                             "vi",
                             "vietnamesekey");
        entries.back()
            .setIcon("vietnamesekey")
            .setLabel("vi")
            .setConfigurable(false);
        return entries;
    }

    void keyEvent(const fcitx::InputMethodEntry& entry,
                  fcitx::KeyEvent&               ke) override {

        if (ke.isRelease()) return;

        fcitx::InputContext* ic = ke.inputContext();
        if (!ic) return;

        // ── DEBUG: log once per InputContext to understand app connection ──
        static std::string last_prog;
        std::string cur_prog = ic->program();
        if (cur_prog != last_prog) {
            last_prog = cur_prog;
            fprintf(stderr, "[VK-DEBUG] program='%s' display='%s' "
                    "preeditEnabled=%d preeditCap=%d\n",
                    cur_prog.c_str(),
                    ic->display().c_str(),
                    ic->isPreeditEnabled() ? 1 : 0,
                    ic->capabilityFlags().test(fcitx::CapabilityFlag::Preedit) ? 1 : 0);
        }

        const uint8_t mode =
            (entry.uniqueName() == "vietnamesekey-vni") ? 1u : 0u;

        FcitxKeySym            sym    = ke.key().sym();
        const fcitx::KeyStates states = ke.key().states();

        // Fix for users accidentally using 'vn' XKB layout instead of 'us'
        if (sym == FcitxKey_dead_belowdot) sym = FcitxKey_period;
        else if (sym == FcitxKey_dead_hook) sym = FcitxKey_slash;
        else if (sym == FcitxKey_dead_tilde) sym = FcitxKey_asciitilde;
        else if (sym == FcitxKey_dead_grave) sym = FcitxKey_grave;
        else if (sym == FcitxKey_dead_acute) sym = FcitxKey_acute;

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
                discard(ic);
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
               fcitx::InputContextEvent&) override {
        // DO NOT flush here. Many apps (especially Electron/Wine/XWayland clients)
        // call reset() between every keystroke, which destroys the composition
        // mid-word (e.g. "biêt" + reset + "s" = "biêts" instead of "biết").
        // The composition will be committed naturally when:
        //   - The user types a word-break (space, punctuation, Enter)
        //   - The user switches apps (deactivate() handles this)
        //   - The user presses Escape (keyEvent handles this)
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
    char             m_utf8[256];  // cached UTF-8 buffer (avoid repeated encoding)
    size_t           m_utf8n = 0;  // cached length
    size_t           m_inline_chars = 0; // codepoints live-committed for no-preedit clients

    // ── Client strategy ──

    static bool contains_ascii_casefold(const std::string& text,
                                        const char* needle) {
        if (!needle || needle[0] == '\0') return true;

        size_t needle_len = 0;
        while (needle[needle_len] != '\0') ++needle_len;
        if (text.size() < needle_len) return false;

        for (size_t i = 0; i + needle_len <= text.size(); ++i) {
            size_t j = 0;
            for (; j < needle_len; ++j) {
                char a = text[i + j];
                char b = needle[j];
                if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
                if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
                if (a != b) break;
            }
            if (j == needle_len) return true;
        }
        return false;
    }

    static bool is_wayland(fcitx::InputContext* ic) {
        const std::string d = ic->display();
        return d.compare(0, 8, "wayland:") == 0 ||
               d.compare(0, 8, "wayland-") == 0;
    }

    static bool force_inline_commit(fcitx::InputContext*) {
        // Use preedit for all apps. The key to making this work is that
        // reset() is a no-op (see above), so the composition survives
        // between keystrokes even on buggy apps.
        return false;
    }

    static bool can_use_preedit(fcitx::InputContext* ic) {
        return ic->isPreeditEnabled() &&
               ic->capabilityFlags().test(fcitx::CapabilityFlag::Preedit) &&
               !force_inline_commit(ic);
    }

    // ── Encode once, use twice ──

    void encode(const char32_t* buf, size_t len) {
        m_utf8n = (len > 0)
            ? encode_utf8(buf, len, m_utf8, sizeof(m_utf8) - 1)
            : 0;
        m_utf8[m_utf8n] = '\0';
    }

    // ── Core display ──
    //
    // Preedit-capable apps use client preedit. Apps without usable preedit
    // need visible inline text, so they get ordered Backspace + commitString
    // replacement instead of deleteSurroundingText, which is flaky on Wayland.
    //
    // Strategy per platform:
    //   X11:          forwardKey(BackSpace) to erase prior inline chars,
    //                 then commitString — reliable synthetic keystrokes.
    //   Wayland:      deleteSurroundingText to erase prior chars,
    //                 then commitString — forwardKey is unreliable on Wayland.

    void show(fcitx::InputContext* ic, const char32_t* obuf, size_t olen) {
        encode(obuf, olen);

        std::string s(m_utf8, m_utf8n);

        if (can_use_preedit(ic)) {
            // App supports preedit → show inline in the app's text field
            fcitx::Text client;
            if (m_utf8n > 0) {
                client.append(s);
                client.setCursor(static_cast<int>(m_utf8n));
            }
            ic->inputPanel().setClientPreedit(client);
        }

        // Always set panel preedit (floating popup near cursor).
        // For apps WITHOUT preedit (like Zalo), this is the ONLY visual feedback.
        // For apps WITH preedit, this is a redundant fallback (harmless).
        fcitx::Text panel;
        if (m_utf8n > 0) {
            panel.append(s);
        }
        ic->inputPanel().setPreedit(panel);

        ic->updatePreedit();
        ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    }

    void show_inline_commit(fcitx::InputContext* ic, size_t olen) {
        // Use synthetic Backspace keys.
        // deleteSurroundingText is ignored by many Electron/Chromium apps on Wayland,
        // which leads to duplicated ghost text (e.g. n -> nno -> nnonó).
        for (size_t i = 0; i < m_inline_chars; ++i) {
            ic->forwardKey(fcitx::Key(FcitxKey_BackSpace));
        }

        if (m_utf8n > 0) {
            ic->commitString(std::string(m_utf8, m_utf8n));
        }
        m_inline_chars = olen;
        clear_preedit(ic);
    }

    // ── Commit final text and clear everything ──

    void commit_buf(fcitx::InputContext* ic, const char32_t* obuf, size_t olen) {
        encode(obuf, olen);
        if (m_inline_chars > 0) {
            show_inline_commit(ic, olen);
            wipe(ic);
            return;
        }
        if (m_utf8n > 0) {
            ic->commitString(std::string(m_utf8, m_utf8n));
        }
        wipe(ic);
    }

    // ── Flush: commit current composition and reset ──

    void flush(fcitx::InputContext* ic) {
        if (engine_.has_composition()) {
            if (m_inline_chars > 0) {
                wipe(ic);
                return;
            }

            char32_t obuf[32];
            size_t olen = 0;
            engine_.get_composition(obuf, olen);
            if (olen > 0) {
                encode(obuf, olen);
                ic->commitString(std::string(m_utf8, m_utf8n));
            }
            wipe(ic);
        }
    }

    // ── Discard composition text ──

    void discard(fcitx::InputContext* ic) {
        for (size_t i = 0; i < m_inline_chars; ++i) {
            ic->forwardKey(fcitx::Key(FcitxKey_BackSpace));
        }
        wipe(ic);
    }

    // ── Wipe all state and clear UI ──

    void wipe(fcitx::InputContext* ic) {
        engine_.reset();
        m_utf8n = 0;
        m_inline_chars = 0;

        clear_preedit(ic);
    }

    void clear_preedit(fcitx::InputContext* ic) {
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
