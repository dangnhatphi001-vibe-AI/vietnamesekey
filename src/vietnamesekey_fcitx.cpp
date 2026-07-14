#include <fcitx/addonfactory.h>
#include <fcitx/addoninstance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <array>
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
        else if (sym == FcitxKey_notsign) sym = FcitxKey_asciitilde; // Fix ¬ to ~

        KeyEventItem item(sym, states, mode);
        if (enqueue(item) && drain_queue(ic)) {
            ke.filterAndAccept();
        }
    }

    void activate(const fcitx::InputMethodEntry&, fcitx::InputContextEvent&) override {}

    void reset(const fcitx::InputMethodEntry&,
               fcitx::InputContextEvent&) override {
        // No-op to protect composition on buggy apps.
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
    struct KeyEventItem {
        KeyEventItem() : sym(FcitxKey_None), states(0), mode(0) {}
        KeyEventItem(FcitxKeySym sym_,
                     fcitx::KeyStates states_,
                     uint8_t mode_)
            : sym(sym_), states(states_), mode(mode_) {}

        FcitxKeySym      sym;
        fcitx::KeyStates states;
        uint8_t          mode;
    };

    fcitx::Instance* instance_;
    ShadowEngine     engine_;

    static constexpr size_t kQueueCapacity = 64;
    std::array<KeyEventItem, kQueueCapacity> queue_{};
    size_t queue_head_ = 0;
    size_t queue_tail_ = 0;
    size_t queue_size_ = 0;
    bool   draining_ = false;

    // Cached buffers for allocation-minimized hot path
    char        m_utf8[256];
    size_t      m_utf8n = 0;
    std::string m_cached_str;

    bool enqueue(const KeyEventItem& item) {
        if (queue_size_ == kQueueCapacity) {
            return false;
        }
        queue_[queue_tail_] = item;
        queue_tail_ = (queue_tail_ + 1) % kQueueCapacity;
        ++queue_size_;
        return true;
    }

    bool pop(KeyEventItem& item) {
        if (queue_size_ == 0) {
            return false;
        }
        item = queue_[queue_head_];
        queue_head_ = (queue_head_ + 1) % kQueueCapacity;
        --queue_size_;
        return true;
    }

    bool drain_queue(fcitx::InputContext* ic) {
        if (draining_) {
            return false;
        }

        bool accepted = false;
        draining_ = true;
        KeyEventItem item{};
        while (pop(item)) {
            accepted = process_queued_key(ic, item) || accepted;
            publish_barrier(ic);
        }
        draining_ = false;
        return accepted;
    }

    bool process_queued_key(fcitx::InputContext* ic, const KeyEventItem& item) {
        const FcitxKeySym      sym = item.sym;
        const fcitx::KeyStates states = item.states;
        const uint8_t          mode = item.mode;

        // Pass through modifier combos untouched
        if ((states & fcitx::KeyState::Ctrl) ||
            (states & fcitx::KeyState::Alt)  ||
            (states & fcitx::KeyState::Super)) {
            flush(ic);
            return false;
        }

        // Escape: discard composition
        if (sym == FcitxKey_Escape) {
            if (engine_.has_composition()) {
                discard(ic);
                return true;
            }
            return false;
        }

        // Enter: commit as-is
        if (sym == FcitxKey_Return || sym == FcitxKey_KP_Enter) {
            if (engine_.has_composition()) {
                flush(ic);
                return true;
            }
            return false;
        }

        // Backspace inside composition
        if (sym == FcitxKey_BackSpace) {
            if (engine_.has_composition()) {
                char32_t obuf[32];
                size_t   olen = 0;
                engine_.process_key(8, mode, obuf, olen);
                show(ic, obuf, olen);
                return true;
            }
            return false;
        }

        // Word break (Space, punctuation): append to composition and commit together
        // This is a CRITICAL fix for "rớt chữ" on fast typing. By committing the word
        // and the space/punctuation in a SINGLE IPC message, we eliminate Wayland races.
        if (ShadowEngine::is_word_break(static_cast<char32_t>(sym), mode)) {
            if (engine_.has_composition()) {
                char32_t obuf[33]; // +1 for the word break char
                size_t   olen = 0;
                engine_.get_composition(obuf, olen);

                // Append the punctuation / space
                if (olen < 32) {
                    obuf[olen++] = static_cast<char32_t>(sym);
                }

                commit_buf(ic, obuf, olen);
                return true;
            }
            return false; // If no composition, pass through natively.
        }

        // Non-printable ASCII (arrows, etc): flush and pass through
        if (sym < 32 || sym > 126) {
            flush(ic);
            return false;
        }

        // ── Main composition path ──
        char32_t obuf[32];
        size_t   olen = 0;
        const bool consumed =
            engine_.process_key(static_cast<char32_t>(sym), mode, obuf, olen);

        if (consumed) {
            show(ic, obuf, olen);
            return true;
        } else {
            if (olen > 0) {
                commit_buf(ic, obuf, olen);
            } else {
                flush(ic);
            }
            return false;
        }
    }

    void encode(const char32_t* buf, size_t len) {
        m_utf8n = (len > 0)
            ? encode_utf8(buf, len, m_utf8, sizeof(m_utf8) - 1)
            : 0;
        m_utf8[m_utf8n] = '\0';
        m_cached_str.assign(m_utf8, m_utf8n); // O(n) copy, zero heap alloc
    }

    static bool has_client_preedit(fcitx::InputContext* ic) {
        return ic->isPreeditEnabled() &&
               ic->capabilityFlags().test(fcitx::CapabilityFlag::Preedit);
    }

    void show(fcitx::InputContext* ic, const char32_t* obuf, size_t olen) {
        encode(obuf, olen);

        if (has_client_preedit(ic)) {
            fcitx::Text client;
            if (m_utf8n > 0) {
                client.append(m_cached_str);
                client.setCursor(static_cast<int>(m_utf8n));
            }
            ic->inputPanel().setClientPreedit(client);
        }

        // Always show floating panel so Zalo/Minecraft users can see the "hộp chữ"
        fcitx::Text panel;
        if (m_utf8n > 0) {
            panel.append(m_cached_str);
        }
        ic->inputPanel().setPreedit(panel);

        ic->updatePreedit();
        ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    }

    void commit_buf(fcitx::InputContext* ic, const char32_t* obuf, size_t olen) {
        encode(obuf, olen);
        if (m_utf8n > 0) {
            ic->commitString(m_cached_str);
        }
        wipe(ic);
    }

    void flush(fcitx::InputContext* ic) {
        if (engine_.has_composition()) {
            char32_t obuf[32];
            size_t olen = 0;
            engine_.get_composition(obuf, olen);
            if (olen > 0) {
                encode(obuf, olen);
                ic->commitString(m_cached_str);
            }
            wipe(ic);
        }
    }

    void discard(fcitx::InputContext* ic) {
        wipe(ic);
    }

    void wipe(fcitx::InputContext* ic) {
        engine_.reset();
        m_utf8n = 0;
        m_cached_str.clear();

        clear_preedit(ic);
    }

    void clear_preedit(fcitx::InputContext* ic) {
        fcitx::Text empty;
        ic->inputPanel().setPreedit(empty);
        ic->inputPanel().setClientPreedit(empty);
        ic->updatePreedit();
        ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    }

    void publish_barrier(fcitx::InputContext* ic) {
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
