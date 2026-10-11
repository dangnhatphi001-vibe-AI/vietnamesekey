// VietnameseKey bench — measures ShadowEngine ns/op
// Run: ./vkey_bench
#include "shadow_engine.h"
#include <chrono>
#include <cstdio>
#include <string>

static std::string to_utf8(const char32_t* s, size_t n) {
    std::string out;
    for (size_t i = 0; i < n; ++i) {
        char32_t cp = s[i];
        if (cp <= 0x7F) out.push_back(char(cp));
        else if (cp <= 0x7FF) { out.push_back(char(0xC0|((cp>>6)&0x1F))); out.push_back(char(0x80|(cp&0x3F))); }
        else if (cp <= 0xFFFF) { out.push_back(char(0xE0|((cp>>12)&0xF))); out.push_back(char(0x80|((cp>>6)&0x3F))); out.push_back(char(0x80|(cp&0x3F))); }
    }
    return out;
}

int main() {
    constexpr int WARMUP = 100000;
    constexpr int ITERS = 1000000;
    const std::string corpus = "xinchaovietnam toi yeu tiengviet duong pho Hanoi dang nhap phi test bench shadow engine ";

    ShadowEngine e;
    char32_t out[ShadowEngine::capacity];
    size_t len = 0;

    // Warmup
    for (int i = 0; i < WARMUP; ++i) {
        char c = corpus[i % corpus.size()];
        e.process_key(static_cast<char32_t>(static_cast<unsigned char>(c)), 0, out, len);
        if (c == ' ') e.reset();
    }
    e.reset();

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < ITERS; ++i) {
        char c = corpus[i % corpus.size()];
        e.process_key(static_cast<char32_t>(static_cast<unsigned char>(c)), 0, out, len);
        if (c == ' ') {
            // word break commits, reset already via process_key
        }
        // prevent optimization
        if (len > 32) std::printf("overflow\n");
    }
    auto t1 = std::chrono::steady_clock::now();
    int64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    double ns_per_key = double(ns) / ITERS;

    // Also measure reconstruct
    e.reset();
    for (char c : std::string("duwowng")) e.process_key(c, 0, out, len);
    t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < ITERS; ++i) {
        char32_t comp[ShadowEngine::capacity]; size_t clen;
        e.get_composition(comp, clen);
        if (clen > 32) std::printf("overflow2\n");
    }
    t1 = std::chrono::steady_clock::now();
    double ns_per_get = double(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()) / ITERS;

    std::printf("VietnameseKey bench (Telex, %d iters, corpus %zu chars):\n", ITERS, corpus.size());
    std::printf("  process_key : %.1f ns/op\n", ns_per_key);
    std::printf("  get_composition: %.1f ns/op\n", ns_per_get);
    std::printf("  sample: duwowng -> %s\n", to_utf8(out, len).c_str());
    return 0;
}
