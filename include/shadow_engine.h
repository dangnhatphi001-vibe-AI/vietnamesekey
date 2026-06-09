#ifndef SHADOW_ENGINE_H
#define SHADOW_ENGINE_H

#include <cstddef>
#include <cstdint>

// ShadowEngine: ultra-low-latency, zero-dynamic-allocation Vietnamese composition engine.
//
// KEY ARCHITECTURAL CHANGE (v2): per-character modifier array.
// The old design used a single global `current_mod` bitmask which made it impossible
// to have ư (u+hook) and ơ (o+hook) in the same syllable simultaneously.
// The new design stores a uint32_t char_mod[32] alongside word_buffer[32], where
// each element independently holds the modifier(s) for that character position.
//
// This correctly handles:
//   • mươi  (m-u-w-o-w-i): u gets MOD_HOOK, then o gets MOD_HOOK independently
//   • dương (d-d-u-o-n-g-w): dd→đ, uo cluster both get MOD_HOOK from single 'w'
//   • ướm   (w-o-w-m-j): ư and ơ both have MOD_HOOK, tone on ơ
class ShadowEngine {
public:
    ShadowEngine();

    // Check if the keycode acts as a word-breaker under the current mode
    static bool is_word_break(char32_t keycode, uint8_t mode);

    // Process one key-press. mode: 0=Telex, 1=VNI.
    // Returns true  → composition was updated (caller should refresh preedit).
    // Returns false → word-break or unhandled key (caller commits pending + passes through).
    bool process_key(char32_t keycode, uint8_t mode,
                     char32_t* output_str, size_t& out_len);

    // Write current composition (UTF-32) into output_str; sets out_len.
    // Called externally when the caller needs to commit on word-break / focus-out.
    void get_composition(char32_t* output_str, size_t& out_len) const;

    // True if there is pending composition text.
    bool has_composition() const { return buffer_length > 0; }

    // Fully clear all state (call on focus-out, deactivate, or hard reset).
    void reset();

private:
    // ── Hot-path state ──────────────────────────────────────────────────────
    char32_t word_buffer[32];  // raw key characters in composition order (case preserved)
    uint32_t char_mod[32];     // per-character modifier bitmask (MOD_HAT | MOD_HOOK | …)
    size_t   buffer_length;    // number of active entries in word_buffer / char_mod

    uint32_t current_tone;     // TONE_* bitmask for the whole syllable (one tone per word)
    char32_t last_tone_key;    // lowercase key that last set current_tone (double-key bypass)

    int      last_mod_target;  // buffer index last modified by a modifier key (-1 = none)
    char32_t last_mod_key;     // lowercase modifier key that set last_mod_target

    // ── Private helpers ─────────────────────────────────────────────────────
    void reset_state();
    bool is_vowel(char32_t c) const;

    // Determine which buffer position should carry the tone diacritic.
    // Returns -1 when no vowel is present. Implements full modern Vietnamese rules.
    int find_tone_target() const;

    // Compose word_buffer + char_mod + current_tone → UTF-32 output.
    void reconstruct_word(char32_t* output_str, size_t& out_len) const;

    // Mode-specific rule handlers. Return true if the key was consumed as a rule.
    bool handle_telex(char32_t keycode, char32_t lower_key, bool has_vowel);
    bool handle_vni  (char32_t keycode, bool has_vowel);
};

#endif // SHADOW_ENGINE_H
