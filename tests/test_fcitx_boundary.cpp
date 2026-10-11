// Exercise the real addon callbacks against a headless Fcitx client.
#include "../src/vietnamesekey_fcitx.cpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>

class TestContext final : public fcitx::InputContext {
public:
    explicit TestContext(fcitx::InputContextManager& manager)
        : InputContext(manager, "electron") { created(); }
    ~TestContext() override { destroy(); }
    const char* frontend() const override { return "test"; }
    std::string committed;
private:
    void commitStringImpl(const std::string& text) override { committed += text; }
    void deleteSurroundingTextImpl(int, unsigned int) override {}
    void forwardKeyImpl(const fcitx::ForwardKeyEvent&) override {}
    void updatePreeditImpl() override {}
};

int main() {
    // Never read or write the user's running Fcitx configuration.
    char temp[] = "/tmp/vkey-boundary-XXXXXX";
    if (!mkdtemp(temp)) return 1;
    setenv("XDG_CONFIG_HOME", temp, 1);
    setenv("XDG_DATA_HOME", temp, 1);
    int failures = 0;
    {
        char name[] = "vkey-boundary-test";
        char* argv[] = {name, nullptr};
        fcitx::Instance instance(1, argv);
        VietnameseKeyEngine engine(&instance);
        TestContext ic(instance.inputContextManager());
        const auto entries = engine.listInputMethods();
        const auto& entry = entries.front();
        auto key = [&](FcitxKeySym sym) {
            fcitx::KeyEvent event(&ic, fcitx::Key(sym));
            engine.keyEvent(entry, event);
            return event.accepted();
        };
        auto check = [&](bool ok, const char* message) {
            if (!ok) { ++failures; std::printf("FAIL %s\n", message); }
        };
        check(key(FcitxKey_a), "letter consumed");
        check(key(FcitxKey_space), "space bundled");
        check(ic.committed == "a ", "one word commit");
        const auto start = fcitx::now(CLOCK_MONOTONIC);
        std::vector<std::unique_ptr<fcitx::EventSourceTime>> checkpoints;
        auto at = [&](uint64_t delay, auto callback) {
            checkpoints.push_back(instance.eventLoop().addTimeEvent(
                CLOCK_MONOTONIC, start + delay, 1,
                [callback](fcitx::EventSourceTime*, uint64_t) {
                    callback(); return false;
                }));
        };
        at(5000, [&] {
            check(!ic.inputPanel().preedit().empty(), "panel survives before 15ms");
        });
        at(100000, [&] {
            check(ic.inputPanel().preedit().empty(), "panel cleared after 15ms");
            key(FcitxKey_a); key(FcitxKey_space); key(FcitxKey_b);
        });
        at(200000, [&] {
            check(ic.inputPanel().preedit().toString() == "b", "new preedit cancels old timer");
            key(FcitxKey_space);
            fcitx::InputContextEvent activate(&ic, fcitx::EventType::InputContextSwitchInputMethod);
            engine.activate(entries.back(), activate);
            check(ic.inputPanel().preedit().empty(), "Raw activation clears deferred panel");
            fcitx::KeyEvent raw(&ic, fcitx::Key(FcitxKey_w));
            engine.keyEvent(entries.back(), raw);
            check(!raw.accepted(), "Raw forwards game controls");

            // Destroying a client with a pending clear must cancel its callback.
            TestContext temporary(instance.inputContextManager());
            for (const auto sym : {FcitxKey_a, FcitxKey_space}) {
                fcitx::KeyEvent event(&temporary, fcitx::Key(sym));
                engine.keyEvent(entry, event);
            }
        });
        at(300000, [&] { instance.eventLoop().exit(); });
        instance.eventLoop().exec();
    }
    std::filesystem::remove_all(temp);
    std::printf("fcitx boundary: %d failures\n", failures);
    return failures ? 1 : 0;
}
