// VietnameseKey ShadowEngine test suite — run via ctest.
// Covers: Telex/VNI composition, tone placement, smart backspace, z-undo,
// double-tap bypass, buffer guards, and a deterministic fuzz smoke test.
#include "shadow_engine.h"
#include "vn_constants.h"

#include <cstdio>
#include <cstring>
#include <string>

static int g_fail = 0;
static int g_pass = 0;

static std::string to_utf8(const char32_t* s, size_t n) {
    std::string out;
    out.reserve(n * 3);
    for (size_t i = 0; i < n; ++i) {
        char32_t cp = s[i];
        if (cp <= 0x7F) {
            out.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

// Type `keys` then read the pending composition.
static std::string type_word(const std::string& keys, uint8_t mode) {
    ShadowEngine e;
    char32_t out[ShadowEngine::capacity];
    size_t len = 0;
    for (char c : keys) {
        e.process_key(static_cast<char32_t>(static_cast<unsigned char>(c)), mode, out, len);
    }
    char32_t comp[ShadowEngine::capacity];
    size_t clen = 0;
    e.get_composition(comp, clen);
    return to_utf8(comp, clen);
}

static void expect_eq(const std::string& keys, uint8_t mode,
                      const std::string& expected) {
    const std::string got = type_word(keys, mode);
    if (got == expected) {
        ++g_pass;
    } else {
        ++g_fail;
        std::printf("FAIL mode=%u keys='%s' got='%s' expected='%s'\n",
                    static_cast<unsigned>(mode), keys.c_str(), got.c_str(),
                    expected.c_str());
    }
}

// Type keys, then a sequence of backspaces, then read composition.
static std::string type_then_backspace(const std::string& keys, int bs,
                                       uint8_t mode) {
    ShadowEngine e;
    char32_t out[ShadowEngine::capacity];
    size_t len = 0;
    for (char c : keys) {
        e.process_key(static_cast<char32_t>(static_cast<unsigned char>(c)), mode, out, len);
    }
    for (int i = 0; i < bs; ++i) {
        e.process_key(8, mode, out, len);
    }
    return to_utf8(out, len);
}

int main() {
    // A delayed d can set the initial stroke, but only an adjacent repeat undoes it.
    expect_eq("ddad", 0, "đad");
    expect_eq("d9a9", 1, "đa");

    // Full-buffer rule keys must return the complete pending word and pass through,
    // rather than reporting success while silently dropping the new character.
    for (const auto& keys : {std::string(32, 'b') + "w",
                             std::string(31, 'b') + "ass",
                             std::string(31, 'b') + "aaa",
                             std::string(31, 'b') + "a11",
                             std::string(31, 'b') + "a66"}) {
        const uint8_t mode = keys.back() >= '0' && keys.back() <= '9' ? 1 : 0;
        ShadowEngine e;
        char32_t out[ShadowEngine::capacity];
        size_t len = 0;
        for (size_t i = 0; i < 32; ++i) e.process_key(keys[i], mode, out, len);
        const std::string pending = to_utf8(out, len);
        const bool consumed = e.process_key(keys[32], mode, out, len);
        if (!consumed && len == 32 && to_utf8(out, len) == pending && !e.has_composition()) {
            ++g_pass;
        } else {
            ++g_fail;
            std::printf("FAIL capacity handoff mode=%u key=%c len=%zu consumed=%d\n",
                        mode, keys[32], len, consumed);
        }
    }

    // ── Telex: vowels & modifiers ────────────────────────────────────────────
    expect_eq("a", 0, "a");
    expect_eq("aw", 0, "\xC4\x83");                 // ă
    expect_eq("aa", 0, "\xC3\xA2");                 // â
    expect_eq("ee", 0, "\xC3\xAA");                 // ê
    expect_eq("oo", 0, "\xC3\xB4");                 // ô
    expect_eq("ow", 0, "\xC6\xA1");                 // ơ
    expect_eq("uw", 0, "\xC6\xB0");                 // ư
    expect_eq("uow", 0, "\xC6\xB0\xC6\xA1");        // ươ
    expect_eq("dd", 0, "\xC4\x91");                 // đ
    expect_eq("ddi", 0, "\xC4\x91" "i");            // đi
    expect_eq("DDi", 0, "\xC4\x90" "i");            // Đi

    // --- Telex: uo cluster -- the horn must be inherited by a following 'o' ---
    // Only "uow" (w after BOTH vowels) used to work. Typing the horn before the
    // o left it bare and silently committed a wrong word. Both orders must now
    // produce the same result, and a redundant 2nd 'w' must be a no-op.
    expect_eq("uwo",      0, "\xC6\xB0\xC6\xA1");                  // horn inherited by following o
    expect_eq("nguwong",  0, "\x6E\x67\xC6\xB0\xC6\xA1\x6E\x67");  // ng u-horn o-horn ng
    expect_eq("tuwong",   0, "\x74\xC6\xB0\xC6\xA1\x6E\x67");      // t + horn + o-horn + ng
    expect_eq("thuwong",  0, "\x74\x68\xC6\xB0\xC6\xA1\x6E\x67");  // th + horn + o-horn + ng
    expect_eq("uwois",    0, "\xC6\xB0\xE1\xBB\x9B\x69");          // u-o-i, horn on both, sac
    expect_eq("uwoij",    0, "\xC6\xB0\xE1\xBB\xA3\x69");          // u-o-i, horn on both, nang
    expect_eq("dduwowjc", 0, "\xC4\x91\xC6\xB0\xE1\xBB\xA3\x63");  // dd + uo + w + j + c
    expect_eq("uow",      0, "\xC6\xB0\xC6\xA1");                  // uow (w after both) still works
    expect_eq("nguowng",  0, "\x6E\x67\xC6\xB0\xC6\xA1\x6E\x67");  // ng + uo + w + ng
    expect_eq("duwowng",  0, "\x64\xC6\xB0\xC6\xA1\x6E\x67");      // d + uo + w + ng
    expect_eq("nguwowif", 0, "\x6E\x67\xC6\xB0\xE1\xBB\x9D\x69");  // redundant 2nd w is a no-op
    expect_eq("muowis",   0, "\x6D\xC6\xB0\xE1\xBB\x9B\x69");      // w after o

    // --- Double-tap bypass must require the SAME key twice in a row ---------
    // last_mod_key / last_tone_key deliberately survive tone keys (so 'z' can still
    // undo a modifier applied before the tone) -- hence the separate last_key gate.
    expect_eq("aasa",     0, "\xE1\xBA\xA5");                      // circumflex+tone, bypass must NOT fire
    expect_eq("ees",      0, "\xE1\xBA\xBF");                      // circumflex+tone
    expect_eq("aas",      0, "\xE1\xBA\xA5");                      // circumflex+tone
    expect_eq("oosa",     0, "\xC3\xB4\xC3\xA1");                  // oa cluster: tone lands on a

    // --- z-undo of a standalone u-horn must restore the literal 'w' ----------
    // The MOD_STANDALONE_W branch was unreachable (MOD_HOOK was tested first),
    // so 'z' left a bare 'u' where it should have restored the 'w'.
    expect_eq("iwz",      0, "\x69\x77");                          // z restores the literal w
    expect_eq("awiwz",    0, "\xC4\x83\x69\x77");                  // z restores the literal w

    // --- Telex: 'o'+'a' cluster keeps the breve on 'a' ---------------------
    expect_eq("hoawc",    0, "\x68\x6F\xC4\x83\x63");              // breve on a, not horn on o
    expect_eq("hoaw",     0, "\x68\x6F\xC4\x83");                  // breve on a
    // ── Telex: tones ─────────────────────────────────────────────────────────
    expect_eq("as", 0, "\xC3\xA1");                 // á
    expect_eq("af", 0, "\xC3\xA0");                 // à
    expect_eq("ar", 0, "\xE1\xBA\xA3");             // ả
    expect_eq("ax", 0, "\xC3\xA3");                 // ã
    expect_eq("aj", 0, "\xE1\xBA\xA1");             // ạ
    expect_eq("tooi", 0, "t\xC3\xB4i");             // tôi
    expect_eq("hoas", 0, "ho\xC3\xA1");             // hoá — tone on 2nd of 'oa'
    expect_eq("quys", 0, "qu\xC3\xBD");             // quý
    expect_eq("gias", 0, "gi\xC3\xA1");             // giá
    expect_eq("tieengs", 0, "ti\xE1\xBA\xBFng");    // tiếng
    expect_eq("duwowng", 0, "d\xC6\xB0\xC6\xA1ng"); // dương
    expect_eq("nguwowif", 0, "ng\xC6\xB0\xE1\xBB\x9Di"); // người
    expect_eq("muwowis", 0, "m\xC6\xB0\xE1\xBB\x9Bi");   // mưới

    // ── Telex: double-tap bypass (literal key) ───────────────────────────────
    expect_eq("aww", 0, "aw");
    expect_eq("ass", 0, "as");
    expect_eq("ddd", 0, "dd");      // bypass reverts đ and appends literal d
    expect_eq("tuww", 0, "tuw");

    // ── Telex: English passthrough ───────────────────────────────────────────
    expect_eq("tw", 0, "tw");      // no vowel → literal w
    expect_eq("hello", 0, "hello");

    // ── z undo ───────────────────────────────────────────────────────────────
    expect_eq("asz", 0, "a");
    expect_eq("aaz", 0, "a");

    // ── Smart backspace ──────────────────────────────────────────────────────
    if (type_then_backspace("aa", 1, 0) == "a") ++g_pass; else { ++g_fail; std::printf("FAIL BS aa\n"); }
    if (type_then_backspace("aw", 1, 0) == "a") ++g_pass; else { ++g_fail; std::printf("FAIL BS aw\n"); }
    if (type_then_backspace("dd", 1, 0) == "d") ++g_pass; else { ++g_fail; std::printf("FAIL BS dd\n"); }
    if (type_then_backspace("ow", 1, 0) == "o") ++g_pass; else { ++g_fail; std::printf("FAIL BS ow\n"); }
    if (type_then_backspace("as", 1, 0) == "a") ++g_pass; else { ++g_fail; std::printf("FAIL BS as\n"); }

    // ── VNI ──────────────────────────────────────────────────────────────────
    expect_eq("a1", 1, "\xC3\xA1");
    expect_eq("a2", 1, "\xC3\xA0");
    expect_eq("a3", 1, "\xE1\xBA\xA3");
    expect_eq("a4", 1, "\xC3\xA3");
    expect_eq("a5", 1, "\xE1\xBA\xA1");
    expect_eq("a6", 1, "\xC3\xA2");                 // â
    expect_eq("a8", 1, "\xC4\x83");                 // ă
    expect_eq("o7", 1, "\xC6\xA1");                 // ơ
    expect_eq("u7", 1, "\xC6\xB0");                 // ư
    expect_eq("d9", 1, "\xC4\x91");                 // đ
    // VNI: same horn-inheritance + double-tap gating as Telex
    expect_eq("a11",      1, "\x61\x31");                          // double-tap bypass
    expect_eq("a61",      1, "\xE1\xBA\xA5");                      // a + 6 + 1
    expect_eq("u7o",      1, "\xC6\xB0\xC6\xA1");                  // horn inherited by following o
    expect_eq("uo7",      1, "\xC6\xB0\xC6\xA1");                  // 7 after both vowels
    expect_eq("u7o1",     1, "\xC6\xB0\xE1\xBB\x9B");              // 7 then o then 1
    expect_eq("ngu7ong",  1, "\x6E\x67\xC6\xB0\xC6\xA1\x6E\x67");  // nguong
    expect_eq("tu7ong",   1, "\x74\xC6\xB0\xC6\xA1\x6E\x67");      // tuong
    // adjacent 6 still bypasses, but a tone key in between must block it
    expect_eq("a66",   1, "a6");                  // adjacent double-tap
    expect_eq("a616",  1, "\xE1\xBA\xA5");      // circumflex survives the tone
    expect_eq("a611",  1, "\xC3\xA2" "1");      // tone double-tap still works
    expect_eq("a818",  1, "\xE1\xBA\xAF");      // breve survives the tone
    expect_eq("a81a8", 1, "\xE1\xBA\xAF\xC4\x83");
    // ── Lookup table guards ──────────────────────────────────────────────────
    if (lookup_vietnamese_char(U'a', TONE_SAC | TONE_HUYEN, MOD_NONE) == U'a') ++g_pass;
    else { ++g_fail; std::printf("FAIL lookup multi-tone guard\n"); }
    if (lookup_vietnamese_char(U'o', TONE_SAC, MOD_HAT | MOD_BREVE) == U'\x00F3') ++g_pass;
    else { ++g_fail; std::printf("FAIL lookup multi-mod guard\n"); }
    // y+horn must resolve to real characters, not a silent fallback to a bare 'y'
    if (lookup_vietnamese_char(U'y', TONE_HUYEN, MOD_HOOK) == U'\x1EF3') ++g_pass;
    else { ++g_fail; std::printf("FAIL lookup y+hook\n"); }
    if (lookup_vietnamese_char(U'Y', TONE_NANG,  MOD_HOOK) == U'\x1EF4') ++g_pass;
    else { ++g_fail; std::printf("FAIL lookup Y+hook\n"); }

    // ── Word-break sanity ────────────────────────────────────────────────────
    if (ShadowEngine::is_word_break(U' ', 0)) ++g_pass; else { ++g_fail; std::printf("FAIL break space\n"); }
    if (!ShadowEngine::is_word_break(U'a', 0)) ++g_pass; else { ++g_fail; std::printf("FAIL break a\n"); }

    // ── Fuzz smoke: deterministic pseudo-random keys must never crash and
    //    composition length must stay within bounds. ─────────────────────────
    {
        uint64_t rng = 0x9E3779B97F4A7C15ull;
        for (int iter = 0; iter < 20000; ++iter) {
            ShadowEngine e;
            for (int k = 0; k < 64; ++k) {
                rng = rng * 6364136223846793005ull + 1442695040888963407ull;
                char32_t key = static_cast<char32_t>((rng >> 33) % 128u);
                char32_t out[40];
                size_t len = 0;
                e.process_key(key, (rng >> 40) & 1 ? 1u : 0u, out, len);
                if (len > 32) {
                    ++g_fail;
                    std::printf("FUZZ FAIL len=%zu iter=%d\n", len, iter);
                    break;
                }
            }
        }
        ++g_pass;
    }

    std::printf("shadow_engine tests: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
