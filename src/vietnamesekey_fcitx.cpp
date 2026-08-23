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
#include <array>
#include <string>
#include <string_view>
#include <vector>
#include <chrono>
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
    struct KeyEventItem {
        KeyEventItem() : sym(FcitxKey_None), states(0), mode(0) {}
        KeyEventItem(FcitxKeySym s, fcitx::KeyStates st, uint8_t m)
            : sym(s), states(st), mode(m) {}
        FcitxKeySym sym;
        fcitx::KeyStates states;
        uint8_t mode;
    };

    VietnameseKeyContext(VietnameseKeyEngine* engine, fcitx::InputContext* ic)
        : engine_(engine), ic_(ic) {
        m_cached_str.reserve(256);
    }

    ShadowEngine& engine() { return engine_state_; }

    void reset() {
        engine_state_.reset();
        clearQueue();
        clearBuffers();
        lastResetMs = 0;
        resetSpamCount = 0;
        if (clearTimer) clearTimer.reset();
    }

    // Queue
    bool enqueue(const KeyEventItem& item) {
        if (queue_size_ == kQueueCapacity) return false;
        queue_[queue_tail_] = item;
        queue_tail_ = (queue_tail_ + 1) % kQueueCapacity;
        ++queue_size_;
        return true;
    }
    bool dequeue(KeyEventItem& item) {
        if (queue_size_ == 0) return false;
        item = queue_[queue_head_];
        queue_head_ = (queue_head_ + 1) % kQueueCapacity;
        --queue_size_;
        return true;
    }
    void clearQueue() {
        queue_head_ = 0;
        queue_tail_ = 0;
        queue_size_ = 0;
        draining_ = false;
    }
    bool isDraining() const { return draining_; }
    void setDraining(bool v) { draining_ = v; }
    size_t queueSize() const { return queue_size_; }
    bool peek(KeyEventItem& out) const {
        if (queue_size_ == 0) return false;
        out = queue_[queue_head_];
        return true;
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

    // Debounce for B1
    uint64_t lastResetMs = 0;
    int resetSpamCount = 0;
    std::unique_ptr<fcitx::EventSourceTime> clearTimer;

    // Cached client profile — ic->program()/display() are immutable per IC.
    // display() returns by value (allocs); computing once removes 2-3 mallocs per keystroke.
    bool profileChecked = false;
    bool brokenBase = false;   // program-name based (zalo/wine/electron/code/chrome)
    bool x11Display = false;   // display starts with x11 (needs live capability check)

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
            for (const char* b : {"zalo", "wine", "electron"}) {
                if (lower.find(b) != std::string_view::npos) { brokenBase = true; return; }
            }
            const std::string disp = ic->display();
            x11Display = disp.find("x11") != std::string::npos;
            if (x11Display &&
                (lower.find("code") != std::string_view::npos ||
                 lower.find("chrome") != std::string_view::npos)) {
                brokenBase = true;
            }
        } else {
            const std::string disp = ic->display();
            x11Display = disp.find("x11:") != std::string::npos;
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
    VietnameseKeyEngine* engine_;
    fcitx::InputContext* ic_;
    ShadowEngine engine_state_;
    static constexpr size_t kQueueCapacity = 64;
    std::array<KeyEventItem, kQueueCapacity> queue_{};
    size_t queue_head_ = 0;
    size_t queue_tail_ = 0;
    size_t queue_size_ = 0;
    bool draining_ = false;
    char m_utf8[256]{};
    size_t m_utf8n = 0;
    std::string m_cached_str;
};

class VietnameseKeyEngine final : public fcitx::InputMethodEngineV2 {
public:
    explicit VietnameseKeyEngine(fcitx::Instance* instance)
        : instance_(instance),
          factory_([this](fcitx::InputContext& ic) {
              return new VietnameseKeyContext(this, &ic);
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

        auto* ctx = ic->propertyFor(&factory_);
        if (!ctx) return;

        VietnameseKeyContext::KeyEventItem item(sym, states, mode);
        // Giữ hộp nổi cho non-native, xử lý đồng bộ từng phím để không nnhnha
        // Queue per-IC chỉ dùng cho burst Wayland (nhiều phím trong 1 tick), còn lại xử lý ngay
        bool accepted = processQueuedKey(ic, ctx, item);
        if (accepted) {
            ke.filterAndAccept();
        }
        // Nếu có burst tồn đọng (do Wayland batch gửi nhiều key trước khi fcitx kịp xử lý), drain gộp để không rớt
        // drainQueue đã coalesce commit+show cho non-native thành 1 IPC
        if (ctx->queueSize() > 0) {
            bool extra = drainQueue(ic, ctx);
            if (extra) ke.filterAndAccept();
        }
    }

    void activate(const fcitx::InputMethodEntry&, fcitx::InputContextEvent&) override {}

    void reset(const fcitx::InputMethodEntry&,
               fcitx::InputContextEvent& ev) override {
        auto* ic = ev.inputContext();
        if (!ic) return;
        auto* ctx = ic->propertyFor(&factory_);
        if (!ctx) return;
        if (!ctx->engine().has_composition()) return;

        // B1 FIX: debounce reset spam from broken clients (Zalo Wine issues XmbResetIC 10+ times per key)
        // Legitimate focus change comes as single reset; spam comes in bursts <300ms
        auto now = std::chrono::steady_clock::now();
        uint64_t nowMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
        if (ctx->lastResetMs != 0 && nowMs - ctx->lastResetMs < 300) {
            ctx->resetSpamCount++;
            if (ctx->resetSpamCount < 5) {
                return; // ignore spam
            }
            // after 5 rapid resets treat as legitimate (e.g., real focus change after spam)
        } else {
            ctx->resetSpamCount = 0;
        }
        ctx->lastResetMs = nowMs;
        // Real reset (focus change, arrow keys via fcitx) should commit pending word like Unikey
        flush(ic, ctx);
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

    bool drainQueue(fcitx::InputContext* ic, VietnameseKeyContext* ctx) {
        if (ctx->isDraining()) return false;
        ctx->setDraining(true);
        bool accepted = false;
        VietnameseKeyContext::KeyEventItem item;
        while (ctx->dequeue(item)) {
            // Coalesce cho non-native: nếu item hiện là word-break commit và còn item sau trong queue
            // thì commit nhưng không clear panel (để show sau ghi đè luôn, giảm 1 IPC, tránh rớt chữ)
            bool isWordBreak = ShadowEngine::is_word_break(static_cast<char32_t>(item.sym), item.mode);
            bool hasNext = ctx->queueSize() > 0;
            bool isNonNative = !has_client_preedit(ic, ctx);
            if (isWordBreak && hasNext && isNonNative && ctx->engine().has_composition()) {
                // Commit gộp: word+space nhưng giữ panel
                char32_t obuf[33];
                size_t olen = 0;
                ctx->engine().get_composition(obuf, olen);
                if (olen < 32) obuf[olen++] = static_cast<char32_t>(item.sym);
                // encode và commit, chỉ reset engine, không clear panel
                ctx->encode(obuf, olen);
                if (ctx->utf8n() > 0) ic->commitString(ctx->cachedStr());
                ctx->engine().reset();
                ctx->clearBuffers();
                // không gọi clear_preedit, để show tiếp theo ghi đè panel luôn
                accepted = true;
                continue;
            }
            bool cur = processQueuedKey(ic, ctx, item);
            accepted = cur || accepted;
        }
        ctx->setDraining(false);
        return accepted;
    }

    bool processQueuedKey(fcitx::InputContext* ic, VietnameseKeyContext* ctx,
                          const VietnameseKeyContext::KeyEventItem& item) {
        const FcitxKeySym sym = item.sym;
        const fcitx::KeyStates states = item.states;
        const uint8_t mode = item.mode;
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
                char32_t obuf[32];
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
                char32_t obuf[33];
                size_t   olen = 0;
                engine.get_composition(obuf, olen);
                if (olen < 32) {
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
        char32_t obuf[32];
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
            // Đặt timer 40ms để clear panel nếu không có phím mới (tránh kẹt chữ cũ)
            // Lưu ý: ctx có thể bị hủy nếu InputContext đóng, nên kiểm tra ic còn valid
            auto* self = this;
            ctx->clearTimer = instance_->eventLoop().addTimeEvent(
                CLOCK_MONOTONIC, 15000, 0,
                [ic, self](fcitx::EventSourceTime*, uint64_t) {
                    if (!ic) return false;
                    auto* c = ic->propertyFor(&self->factory_);
                    if (c) {
                        if (!c->engine().has_composition()) {
                            self->clear_preedit(ic);
                        }
                        c->clearTimer.reset();
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
            char32_t obuf[32];
            size_t olen = 0;
            ctx->engine().get_composition(obuf, olen);
            if (olen > 0) {
                ctx->encode(obuf, olen);
                ic->commitString(ctx->cachedStr());
            }
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
