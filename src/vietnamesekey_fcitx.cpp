#include <fcitx/addonfactory.h>
#include <fcitx/addoninstance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <string>
#include <string_view>
#include <vector>
#include <cctype>
#include <memory>
#include <fcitx/inputpanel.h>
#include <fcitx/text.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/eventloopinterface.h>

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

class VietnameseKeyEngine;

class VietnameseKeyContext : public fcitx::InputContextProperty {
public:
    VietnameseKeyContext() { m_cached_str.reserve(256); }

    ShadowEngine& engine() { return engine_state_; }

    void reset() {
        engine_state_.reset();
        clearBuffers();
        if (clearTimer) clearTimer.reset();
    }

    // Buffers per-IC — fixes B3 race where global m_utf8 was shared
    void encode(const char32_t* buf, size_t len) {
        m_utf8n = (len > 0) ? encode_utf8(buf, len, m_utf8, sizeof(m_utf8)-1) : 0;
        m_utf8[m_utf8n] = '\0';
        m_cached_str.assign(m_utf8, m_utf8n);
    }
    const std::string& cachedStr() const { return m_cached_str; }
    size_t utf8n() const { return m_utf8n; }
    const char* utf8() const { return m_utf8; }
    void clearBuffers() {
        m_utf8n = 0;
        m_cached_str.clear();
        m_utf8[0] = '\0';
    }

    // 15ms one-shot that clears the floating panel for non-native clients
    std::unique_ptr<fcitx::EventSourceTime> clearTimer;

    // Cached client profile — ic->program()/display() are immutable per IC.
    // display() returns by value (allocs); computing once removes 2-3 mallocs per keystroke.
    bool profileChecked = false;
    bool brokenBase = false;   // program-name based (zalo/wine/electron/code/chrome)
    bool x11Display = false;   // display reports x11 (needs live capability check)

    // fcitx5 reports the display as "x11::0" (type:string). One predicate for both branches
    // below — they used to disagree (bare "x11" vs "x11:"), misclassifying clients.
    static bool is_x11_display(const std::string& disp) {
        return disp.find("x11:") != std::string::npos;
    }

    void initClientProfile(fcitx::InputContext* ic) {
        if (profileChecked) return;
        profileChecked = true;
        const std::string &prog = ic->program();
        if (!prog.empty()) {
            char buf[128];
            size_t n = prog.size() < sizeof(buf) ? prog.size() : sizeof(buf) - 1;
            for (size_t i = 0; i < n; ++i) {
                buf[i] = static_cast<char>(::tolower(static_cast<unsigned char>(prog[i])));
            }
            const std::string_view lower(buf, n);
            // NOTE: "wine" is deliberately NOT in this list. Every Wine/Proton process
            // reports a program name containing "wine", so including it forced the
            // isolated floating-panel path on EVERY Proton game — keys got parked in the
            // panel and never reached the game (movement/controls dead). The panel is
            // only a fallback for clients that *claim* preedit support but render it
            // wrong, which is a real problem for Zalo/Electron and not for games.
            for (const char* b : {"zalo", "electron"}) {
                if (lower.find(b) != std::string_view::npos) { brokenBase = true; return; }
            }
            x11Display = is_x11_display(ic->display());
            if (x11Display &&
                (lower.find("code") != std::string_view::npos ||
                 lower.find("chrome") != std::string_view::npos)) {
                brokenBase = true;
            }
        } else {
            x11Display = is_x11_display(ic->display());
        }
    }

    bool isBrokenClient(fcitx::InputContext* ic) {
        initClientProfile(ic);
        if (brokenBase) return true;
        // XWayland clients with flaky preedit: live bitmask check, no allocation
        if (x11Display &&
            !ic->capabilityFlags().test(fcitx::CapabilityFlag::FormattedPreedit)) {
            return true;
        }
        return false;
    }

private:
    ShadowEngine engine_state_;
    char m_utf8[256]{};
    size_t m_utf8n = 0;
    std::string m_cached_str;
};

class VietnameseKeyEngine final : public fcitx::InputMethodEngineV2 {
public:
    explicit VietnameseKeyEngine(fcitx::Instance* instance)
        : instance_(instance),
          factory_([](fcitx::InputContext&) {
              return new VietnameseKeyContext();
          }) {
        instance_->inputContextManager().registerProperty("vietnamesekey_state", &factory_);
    }

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

        // VNI (mode 1) is still implemented and tested in ShadowEngine, but is no
        // longer exposed as an input method: it was unusable (the user reported it
        // behaving like Telex) and Telex covers their typing. Re-enabling it is just
        // adding this entry plus the matching .conf file.
        //
        // Gaming / raw pass-through. Consumes NOTHING: every key goes straight to the
        // client, no composition, no preedit, no floating panel. Select this IM while a
        // game has the keyboard — Proton/Wine games talk XIM, which has no client-side
        // preedit, so Telex would park keystrokes in the input panel and the game
        // would never see WASD.
        entries.emplace_back("vietnamesekey-raw",
                             "VietnameseKey-Raw (gaming)",
                             "vi",
                             "vietnamesekey");
        entries.back()
            .setIcon("vietnamesekey")
            .setLabel("raw")
            .setConfigurable(false);
        return entries;
    }

    void keyEvent(const fcitx::InputMethodEntry& entry,
                  fcitx::KeyEvent&               ke) override {

        if (ke.isRelease()) return;

        fcitx::InputContext* ic = ke.inputContext();
        if (!ic) return;

        // Raw pass-through: drop any pending composition and forward every key.
        if (entry.uniqueName() == "vietnamesekey-raw") {
            if (auto* c = ic->propertyFor(&factory_)) c->engine().reset();
            return;   // not consumed -> client receives the key verbatim
        }

        // Only Telex is exposed, so the engine is always driven in mode 0.
        constexpr uint8_t mode = 0;

        FcitxKeySym            sym    = ke.key().sym();
        const fcitx::KeyStates states = ke.key().states();

        // Fix for users accidentally using 'vn' XKB layout instead of 'us'
        if (sym == FcitxKey_dead_belowdot) sym = FcitxKey_period;
        else if (sym == FcitxKey_dead_hook) sym = FcitxKey_slash;
        else if (sym == FcitxKey_dead_tilde) sym = FcitxKey_asciitilde;
        else if (sym == FcitxKey_dead_grave) sym = FcitxKey_grave;
        else if (sym == FcitxKey_dead_acute) sym = FcitxKey_acute;

        auto* ctx = ic->propertyFor(&factory_);
        if (!ctx) return;

        if (processKey(ic, ctx, sym, states, mode)) {
            ke.filterAndAccept();
        }
    }

    // Switching into this addon (e.g. Telex -> Raw) must not leave a stale floating
    // panel behind: for a game that panel is the last thing on screen and it would
    // never be cleared because no further key is ever consumed.
    void activate(const fcitx::InputMethodEntry&, fcitx::InputContextEvent& ev) override {
        auto* ic = ev.inputContext();
        if (!ic) return;
        auto* ctx = ic->propertyFor(&factory_);
        if (!ctx) return;
        if (!ctx->engine().has_composition() && ctx->utf8n() == 0 && !ctx->clearTimer) return;
        flush(ic, ctx);
    }

    void reset(const fcitx::InputMethodEntry&,
               fcitx::InputContextEvent& ev) override {
        auto* ic = ev.inputContext();
        if (!ic) return;
        // Real reset (focus change, arrow keys via fcitx) should commit pending word like Unikey.
        // flush() already no-ops when there is nothing pending.
        // NOTE: the old 300ms/5-strike "B1" debounce here was unreachable — the
        // !has_composition() guard ran first and the first reset already flushed, so
        // resetSpamCount never incremented. Dropped as dead weight.
        flush(ic);
    }

    void deactivate(const fcitx::InputMethodEntry& entry,
                    fcitx::InputContextEvent&       ev) override {
        if (auto* ic = ev.inputContext()) {
            flush(ic);
        }
        InputMethodEngineV2::deactivate(entry, ev);
    }

    std::string subModeLabelImpl(const fcitx::InputMethodEntry&, fcitx::InputContext&) override {
        return "";
    }

private:
    fcitx::Instance* instance_;
    fcitx::FactoryFor<VietnameseKeyContext> factory_;

    bool processKey(fcitx::InputContext* ic, VietnameseKeyContext* ctx,
                    FcitxKeySym sym, fcitx::KeyStates states, uint8_t mode) {
        auto& engine = ctx->engine();

        // 1. Pass through modifier combos untouched
        if ((states & fcitx::KeyState::Ctrl) ||
            (states & fcitx::KeyState::Alt)  ||
            (states & fcitx::KeyState::Super)) {
            flush(ic, ctx);
            return false;
        }

        // 2. Escape: discard composition
        if (sym == FcitxKey_Escape) {
            if (engine.has_composition()) {
                discard(ic, ctx);
                return true;
            }
            return false;
        }

        // 3. Enter: commit as-is
        if (sym == FcitxKey_Return || sym == FcitxKey_KP_Enter) {
            if (engine.has_composition()) {
                flush(ic, ctx);
                return true;
            }
            return false;
        }

        // 4. Backspace inside composition — delegate to ShadowEngine smart backspace
        if (sym == FcitxKey_BackSpace) {
            if (engine.has_composition()) {
                char32_t obuf[ShadowEngine::capacity];
                size_t   olen = 0;
                engine.process_key(8, mode, obuf, olen);
                if (olen > 0) {
                    show(ic, ctx, obuf, olen);
                } else {
                    wipe(ic, ctx);
                }
                return true;
            }
            return false;
        }

        // 5. Word break (Space, punctuation): append to composition and commit together
        if (ShadowEngine::is_word_break(static_cast<char32_t>(sym), mode)) {
            if (engine.has_composition()) {
                char32_t obuf[ShadowEngine::capacity + 1];
                size_t   olen = 0;
                engine.get_composition(obuf, olen);
                if (olen < ShadowEngine::capacity + 1) {
                    obuf[olen++] = static_cast<char32_t>(sym);
                }
                commit_buf(ic, ctx, obuf, olen);
                return true;
            }
            return false;
        }

        // 6. Non-printable ASCII (arrows, etc): flush and pass through
        if (sym < 32 || sym > 126) {
            flush(ic, ctx);
            return false;
        }

        // 7. Main composition path
        char32_t obuf[ShadowEngine::capacity];
        size_t   olen = 0;
        const bool consumed =
            engine.process_key(static_cast<char32_t>(sym), mode, obuf, olen);

        if (consumed) {
            show(ic, ctx, obuf, olen);
            return true;
        } else {
            if (olen > 0) {
                commit_buf(ic, ctx, obuf, olen);
            } else {
                flush(ic, ctx);
            }
            return false;
        }
    }

    static bool has_client_preedit(fcitx::InputContext* ic, VietnameseKeyContext* ctx) {
        if (!ic->isPreeditEnabled()) return false;
        if (!ic->capabilityFlags().test(fcitx::CapabilityFlag::Preedit)) return false;
        if (ctx->isBrokenClient(ic)) return false;
        return true;
    }

    void show(fcitx::InputContext* ic, VietnameseKeyContext* ctx, const char32_t* obuf, size_t olen) {
        // Hủy timer clear cũ vì có chữ mới ghi đè panel
        if (ctx->clearTimer) ctx->clearTimer.reset();
        ctx->encode(obuf, olen);
        const std::string &str = ctx->cachedStr();
        size_t n = ctx->utf8n();

        if (has_client_preedit(ic, ctx)) {
            fcitx::Text client;
            if (n > 0) {
                client.append(str);
                client.setCursor(static_cast<int>(n));
            }
            ic->inputPanel().setClientPreedit(client);
            fcitx::Text empty;
            ic->inputPanel().setPreedit(empty);
        } else {
            fcitx::Text empty;
            ic->inputPanel().setClientPreedit(empty);
            fcitx::Text panel;
            if (n > 0) {
                panel.append(str);
            }
            ic->inputPanel().setPreedit(panel);
        }
        ic->updatePreedit();
        ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    }

    void commit_buf(fcitx::InputContext* ic, VietnameseKeyContext* ctx, const char32_t* obuf, size_t olen) {
        ctx->encode(obuf, olen);
        if (ctx->utf8n() > 0) {
            ic->commitString(ctx->cachedStr());
        }
        if (has_client_preedit(ic, ctx)) {
            wipe(ic, ctx);
        } else {
            // Non-native: giữ hộp nổi, không clear ngay để tránh 2 IPC liên tiếp gây rớt chữ khi gõ nhanh
            // Chỉ reset engine, panel sẽ được show() kế tiếp ghi đè (coalesce). Nếu dừng gõ, timer sẽ clear.
            ctx->engine().reset();
            ctx->clearBuffers();
            // Hủy timer cũ nếu có
            ctx->clearTimer.reset();
            // Đặt timer 15ms để clear panel nếu không có phím mới (tránh kẹt chữ cũ).
            // ic->watch() là weak handle: nếu InputContext bị hủy trước khi timer nổ
            // (client đóng tab, đổi IM) thì get() trả nullptr thay vì deref vùng nhớ đã free.
            auto icWatch = ic->watch();
            auto* self = this;
            // Bound timer coalescing to 1ms; sd-event accuracy=0 defaults to 250ms.
            ctx->clearTimer = instance_->eventLoop().addTimeEvent(
                CLOCK_MONOTONIC, fcitx::now(CLOCK_MONOTONIC) + 15000, 1000,
                [icWatch, self](fcitx::EventSourceTime* source, uint64_t) {
                    fcitx::InputContext* live = icWatch.get();
                    if (!live) return false;
                    auto* c = live->propertyFor(&self->factory_);
                    if (c) {
                        if (!c->engine().has_composition()) {
                            self->clear_preedit(live);
                        }
                        // Không reset clearTimer ở đây để tránh tự hủy timer trong callback
                        // Timer là one-shot (return false), sẽ được hủy khi ctx bị hủy
                        // hoặc khi show()/commit tiếp theo ghi đè
                        (void)source;
                    }
                    return false;
                });
        }
    }

    void flush(fcitx::InputContext* ic, VietnameseKeyContext* ctx = nullptr) {
        if (!ic) return;
        if (!ctx) {
            ctx = ic->propertyFor(&factory_);
        }
        if (ctx && ctx->engine().has_composition()) {
            char32_t obuf[ShadowEngine::capacity];
            size_t olen = 0;
            ctx->engine().get_composition(obuf, olen);
            if (olen > 0) {
                ctx->encode(obuf, olen);
                ic->commitString(ctx->cachedStr());
            }
            wipe(ic, ctx);
        } else if (ctx && ctx->clearTimer) {
            // A just-committed word can still have a deferred floating panel.
            // Clear it on focus/IM switches even though the engine is empty.
            wipe(ic, ctx);
        }
    }

    void discard(fcitx::InputContext* ic, VietnameseKeyContext* ctx = nullptr) {
        if (!ctx) {
            ctx = ic->propertyFor(&factory_);
        }
        wipe(ic, ctx);
    }

    void wipe(fcitx::InputContext* ic, VietnameseKeyContext* ctx) {
        if (ctx) {
            ctx->reset();
        }
        clear_preedit(ic);
    }

    void clear_preedit(fcitx::InputContext* ic) {
        fcitx::Text empty;
        ic->inputPanel().setPreedit(empty);
        ic->inputPanel().setClientPreedit(empty);
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
