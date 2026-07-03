#include "shadow_engine.h"
#include "vn_constants.h"

// Standing bitmask for standalone 'w' key insertion
static constexpr uint32_t MOD_STANDALONE_W = 1u << 9;

// ──────────────────────────────────────────────────────────────────────────────
//  Utility
// ──────────────────────────────────────────────────────────────────────────────
static inline char32_t lower32(char32_t c) {
    return (c >= U'A' && c <= U'Z') ? (c - U'A' + U'a') : c;
}

// ──────────────────────────────────────────────────────────────────────────────
//  Construction / reset
// ──────────────────────────────────────────────────────────────────────────────
ShadowEngine::ShadowEngine() { reset_state(); }
void ShadowEngine::reset()   { reset_state(); }

void ShadowEngine::reset_state() {
    __builtin_memset(word_buffer, 0, sizeof(word_buffer));
    __builtin_memset(char_mod, 0, sizeof(char_mod));
    buffer_length   = 0;
    current_tone    = TONE_NONE;
    last_tone_key   = U'\0';
    last_mod_target = -1;
    last_mod_key    = U'\0';
}

// ──────────────────────────────────────────────────────────────────────────────
//  Vowel classifier & Word Break Utility
// ──────────────────────────────────────────────────────────────────────────────
bool ShadowEngine::is_vowel(char32_t c) const {
    const char32_t l = lower32(c);
    if (l < U'a' || l > U'z') return false;
    // a=0, e=4, i=8, o=14, u=20, y=24
    // 1<<0 | 1<<4 | 1<<8 | 1<<14 | 1<<20 | 1<<24 = 0x1104111
    return (0x1104111 >> (l - U'a')) & 1;
}

bool ShadowEngine::is_word_break(char32_t keycode, uint8_t mode) {
    if (keycode > 126u) return false;
    const char c = static_cast<char>(keycode);
    if (c == ' ') return true;
    if (c >= '!' && c <= '/') return true;
    if (c >= ':' && c <= '@') return true;
    if (c >= '[' && c <= '`') return true;
    if (c >= '{' && c <= '~') return true;
    if (mode == 0 && c >= '0' && c <= '9') return true;  // Telex: digits break
    if (mode == 1 && c == '0')             return true;  // VNI: only '0' breaks
    return false;
}

// ──────────────────────────────────────────────────────────────────────────────
//  Tone-target resolution — full modern Vietnamese placement rules
// ──────────────────────────────────────────────────────────────────────────────
int ShadowEngine::find_tone_target() const {
    int    vidx[32];
    size_t vcnt = 0;

    // Detect qu- / gi- glide absorption
    bool skip_pos1 = false;
    if (buffer_length >= 2) {
        const char32_t c0 = lower32(word_buffer[0]);
        const char32_t c1 = lower32(word_buffer[1]);
        if ((c0 == U'q' && c1 == U'u') || (c0 == U'g' && c1 == U'i')) {
            for (size_t k = 2; k < buffer_length; ++k) {
                if (is_vowel(word_buffer[k])) { skip_pos1 = true; break; }
            }
        }
    }

    for (size_t i = 0; i < buffer_length; ++i) {
        if (!is_vowel(word_buffer[i])) continue;
        if (i == 1 && skip_pos1) continue;
        vidx[vcnt++] = static_cast<int>(i);
    }

    if (vcnt == 0) return -1;
    if (vcnt == 1) return vidx[0];

    if (vcnt == 2) {
        const char32_t v0 = lower32(word_buffer[vidx[0]]);
        const char32_t v1 = lower32(word_buffer[vidx[1]]);
        if ((v0 == U'o' && (v1 == U'a' || v1 == U'e')) ||
            (v0 == U'u' && (v1 == U'y' || v1 == U'o'))) {
            return vidx[1];   // oa / oe / uy / uo → 2nd vowel always
        }
        if (static_cast<size_t>(vidx[1]) < buffer_length - 1) {
            return vidx[1];   // closed syllable → 2nd vowel
        }
        return vidx[0];       // open syllable → 1st vowel
    }

    // vcnt >= 3
    const char32_t v0 = lower32(word_buffer[vidx[0]]);
    const char32_t v1 = lower32(word_buffer[vidx[1]]);
    const char32_t v2 = lower32(word_buffer[vidx[2]]);
    if (v0 == U'u' && v1 == U'y' && v2 == U'e') {
        return vidx[2];       // uyê cluster → tone on ê
    }
    return vidx[1];           // general triphthong → middle vowel
}

// ──────────────────────────────────────────────────────────────────────────────
//  Word reconstruction: word_buffer + char_mod + current_tone → UTF-32 output
// ──────────────────────────────────────────────────────────────────────────────
void ShadowEngine::reconstruct_word(char32_t* output_str, size_t& out_len) const {
    const int tone_target = find_tone_target();
    out_len = 0;
    for (size_t i = 0; i < buffer_length; ++i) {
        const char32_t c  = word_buffer[i];
        const char32_t lc = lower32(c);
        char32_t mapped   = c;
        if (is_vowel(c)) {
            const uint32_t mod  = char_mod[i];
            const uint32_t tone = (static_cast<int>(i) == tone_target)
                                    ? current_tone : TONE_NONE;
            mapped = lookup_vietnamese_char(c, tone, mod);
        } else if (lc == U'd') {
            mapped = lookup_vietnamese_char(c, TONE_NONE, char_mod[i]);
        }
        output_str[out_len++] = mapped;
    }
}

void ShadowEngine::get_composition(char32_t* output_str, size_t& out_len) const {
    reconstruct_word(output_str, out_len);
}

// ──────────────────────────────────────────────────────────────────────────────
//  Lookbehind Interception Helpers
// ──────────────────────────────────────────────────────────────────────────────
bool ShadowEngine::apply_lookbehind_consonant(char32_t keycode, char32_t lower_key, uint8_t mode) {
    if (buffer_length == 0) return false;

    // Intercept modifier for initial 'd' at index 0 ( Telex 'd' or VNI '9' )
    if (lower32(word_buffer[0]) == U'd') {
        const bool is_telex_d = (mode == 0 && lower_key == U'd');
        const bool is_vni_9   = (mode == 1 && keycode == U'9');

        if (is_telex_d || is_vni_9) {
            if (!(char_mod[0] & MOD_D)) {
                char_mod[0] |= MOD_D;
                last_mod_key    = is_telex_d ? U'd' : U'9';
                last_mod_target = 0;
            } else {
                // Undo logic: revert 'đ' to 'd' and append the new modifier key
                char_mod[0] &= ~MOD_D;
                last_mod_key    = U'\0';
                last_mod_target = -1;
                if (buffer_length < 32) word_buffer[buffer_length++] = keycode;
            }
            return true;
        }
    }
    return false;
}

bool ShadowEngine::apply_vowel_tone(char32_t keycode, char32_t lower_key, uint8_t mode, bool has_vowel) {
    if (mode == 0) {
        return handle_telex(keycode, lower_key, has_vowel);
    } else {
        return handle_vni(keycode, has_vowel);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
//  Telex rule handler
// ──────────────────────────────────────────────────────────────────────────────
bool ShadowEngine::handle_telex(char32_t keycode, char32_t lower_key, bool has_vowel) {
    // ── Tone keys: s=sắc  f=huyền  r=hỏi  x=ngã  j=nặng ────────────────────
    uint32_t target_tone = TONE_NONE;
    if      (lower_key == U's') target_tone = TONE_SAC;
    else if (lower_key == U'f') target_tone = TONE_HUYEN;
    else if (lower_key == U'r') target_tone = TONE_HOI;
    else if (lower_key == U'x') target_tone = TONE_NGA;
    else if (lower_key == U'j') target_tone = TONE_NANG;

    if (target_tone != TONE_NONE && has_vowel) {
        if (current_tone == target_tone && last_tone_key == lower_key) {
            // Double-key bypass: strip tone, push literal character.
            current_tone  = TONE_NONE;
            last_tone_key = U'\0';
            if (buffer_length < 32) word_buffer[buffer_length++] = keycode;
        } else {
            current_tone  = target_tone;
            last_tone_key = lower_key;
        }
        return true;
    }

    // ── Circumflex: aa → â  /  ee → ê  /  oo → ô ───────────────────────────
    if ((lower_key == U'a' || lower_key == U'e' || lower_key == U'o') &&
         buffer_length > 0 &&
         lower32(word_buffer[buffer_length - 1]) == lower_key) {

        const int target = static_cast<int>(buffer_length) - 1;
        if ((char_mod[target] & MOD_HAT) &&
            last_mod_key == lower_key && last_mod_target == target) {
            // Double-key bypass.
            char_mod[target] &= ~MOD_HAT;
            last_mod_key    = U'\0';
            last_mod_target = -1;
            if (buffer_length < 32) word_buffer[buffer_length++] = keycode;
        } else {
            // Clear conflicting modifiers before setting HAT.
            if (lower_key == U'a') char_mod[target] &= ~MOD_BREVE;
            if (lower_key == U'o') char_mod[target] &= ~MOD_HOOK;
            char_mod[target] |= MOD_HAT;
            last_mod_key    = lower_key;
            last_mod_target = target;
        }
        return true;
    }

    // ── D-slash: dd → đ ──────────────────────────────────────────────────────
    if (lower_key == U'd' &&
        buffer_length > 0 &&
        lower32(word_buffer[buffer_length - 1]) == U'd') {

        const int target = static_cast<int>(buffer_length) - 1;
        if ((char_mod[target] & MOD_D) &&
            last_mod_key == U'd' && last_mod_target == target) {
            char_mod[target] &= ~MOD_D;
            last_mod_key    = U'\0';
            last_mod_target = -1;
            if (buffer_length < 32) word_buffer[buffer_length++] = keycode;
        } else {
            char_mod[target] |= MOD_D;
            last_mod_key    = U'd';
            last_mod_target = target;
        }
        return true;
    }

    // ── Hook / Breve: w ──────────────────────────────────────────────────────
    if (lower_key == U'w') {
        // Find rightmost u or o that doesn't have MOD_HOOK yet.
        int hook_target = -1;
        for (int i = static_cast<int>(buffer_length) - 1; i >= 0; --i) {
            const char32_t l = lower32(word_buffer[i]);
            if ((l == U'u' || l == U'o') && !(char_mod[i] & MOD_HOOK)) {
                if (l == U'o' && i + 1 < static_cast<int>(buffer_length) && lower32(word_buffer[i + 1]) == U'a') {
                    continue;
                }
                if (l == U'u' && i >= 2 && lower32(word_buffer[i - 1]) == U'o' && lower32(word_buffer[i - 2]) == U'u') {
                    continue;
                }
                hook_target = i;
                break;
            }
        }

        int breve_target = -1;
        for (int i = static_cast<int>(buffer_length) - 1; i >= 0; --i) {
            if (lower32(word_buffer[i]) == U'a' && !(char_mod[i] & MOD_BREVE)) {
                breve_target = i;
                break;
            }
        }

        // Priority 1: apply MOD_HOOK to rightmost unmodified u or o.
        // Only if buffer already has a vowel (not starting English words with w)
        if (hook_target >= 0 && has_vowel) {
            const char32_t target_l = lower32(word_buffer[hook_target]);
            int left_idx = hook_target - 1;
            char32_t left_l = (left_idx >= 0) ? lower32(word_buffer[left_idx]) : U'\0';

            if ((target_l == U'o' && left_l == U'u') || 
                (target_l == U'u' && left_l == U'o')) {
                char_mod[hook_target] |= MOD_HOOK;
                char_mod[hook_target] &= ~MOD_HAT;
                char_mod[left_idx] |= MOD_HOOK;
                char_mod[left_idx] &= ~MOD_HAT;
                last_mod_target = hook_target;
            } else if (target_l == U'u' && left_l == U'u') {
                char_mod[left_idx] |= MOD_HOOK;
                char_mod[left_idx] &= ~MOD_HAT;
                last_mod_target = left_idx;
            } else {
                char_mod[hook_target] |= MOD_HOOK;
                char_mod[hook_target] &= ~MOD_HAT;
                last_mod_target = hook_target;
            }
            last_mod_key = U'w';
            return true;
        }

        // Priority 2: apply MOD_BREVE to rightmost unmodified 'a'.
        // Only if buffer already has a vowel
        if (breve_target >= 0 && has_vowel) {
            char_mod[breve_target] |= MOD_BREVE;
            char_mod[breve_target] &= ~MOD_HAT;
            last_mod_key    = U'w';
            last_mod_target = breve_target;
            return true;
        }

        // Priority 3: Double-key bypass.
        // Revert last modification made by 'w' if any, then insert/replace.
        if (last_mod_key == U'w' && last_mod_target >= 0) {
            const char32_t target_char = lower32(word_buffer[last_mod_target]);
            if (char_mod[last_mod_target] & MOD_STANDALONE_W) {
                if (last_mod_target == 0) {
                    word_buffer[0] = (word_buffer[0] == U'U') ? U'W' : U'w';
                    char_mod[0] = 0u;
                } else {
                    char_mod[last_mod_target] &= ~(MOD_HOOK | MOD_STANDALONE_W);
                    if (buffer_length < 32) word_buffer[buffer_length++] = keycode;
                }
            } else {
                if (target_char == U'u' || target_char == U'o') {
                    for (size_t i = 0; i < buffer_length; ++i) {
                        const char32_t l = lower32(word_buffer[i]);
                        if (l == U'u' || l == U'o') char_mod[i] &= ~MOD_HOOK;
                    }
                } else if (target_char == U'a') {
                    for (size_t i = 0; i < buffer_length; ++i) {
                        if (lower32(word_buffer[i]) == U'a') char_mod[i] &= ~MOD_BREVE;
                    }
                }
                if (buffer_length < 32) word_buffer[buffer_length++] = keycode;
            }
            last_mod_key    = U'\0';
            last_mod_target = -1;
            return true;
        }

        // Priority 4: standalone 'w' → insert u with MOD_HOOK and MOD_STANDALONE_W (ư).
        // Only create ư if we already have a vowel in buffer (actively composing Vietnamese).
        // If buffer is empty or has no vowel, insert literal 'w' for English typing.
        if (buffer_length < 32) {
            if (has_vowel) {
                word_buffer[buffer_length] = (keycode == U'W') ? U'U' : U'u';
                char_mod[buffer_length]    = MOD_HOOK | MOD_STANDALONE_W;
                last_mod_key    = U'w';
                last_mod_target = static_cast<int>(buffer_length);
                buffer_length++;
            } else {
                word_buffer[buffer_length] = keycode;
                char_mod[buffer_length]    = 0u;
                buffer_length++;
            }
        }
        return true;
    }

    return false;
}

// ──────────────────────────────────────────────────────────────────────────────
//  VNI rule handler
// ──────────────────────────────────────────────────────────────────────────────
bool ShadowEngine::handle_vni(char32_t keycode, bool has_vowel) {
    // ── Tone keys: 1=sắc  2=huyền  3=hỏi  4=ngã  5=nặng ────────────────────
    uint32_t target_tone = TONE_NONE;
    if      (keycode == U'1') target_tone = TONE_SAC;
    else if (keycode == U'2') target_tone = TONE_HUYEN;
    else if (keycode == U'3') target_tone = TONE_HOI;
    else if (keycode == U'4') target_tone = TONE_NGA;
    else if (keycode == U'5') target_tone = TONE_NANG;

    if (target_tone != TONE_NONE && has_vowel) {
        if (current_tone == target_tone && last_tone_key == keycode) {
            current_tone  = TONE_NONE;
            last_tone_key = U'\0';
            if (buffer_length < 32) word_buffer[buffer_length++] = keycode;
        } else {
            current_tone  = target_tone;
            last_tone_key = keycode;
        }
        return true;
    }

    // ── 6 → circumflex (a/e/o → â/ê/ô): rightmost eligible vowel ────────────
    if (keycode == U'6') {
        int hat_target = -1;
        for (int i = static_cast<int>(buffer_length) - 1; i >= 0; --i) {
            const char32_t l = lower32(word_buffer[i]);
            if (l == U'a' || l == U'e' || l == U'o') { hat_target = i; break; }
        }
        if (hat_target >= 0) {
            if ((char_mod[hat_target] & MOD_HAT) &&
                last_mod_key == U'6' && last_mod_target == hat_target) {
                char_mod[hat_target] &= ~MOD_HAT;
                last_mod_key    = U'\0';
                last_mod_target = -1;
                if (buffer_length < 32) word_buffer[buffer_length++] = keycode;
            } else {
                if (lower32(word_buffer[hat_target]) == U'a') char_mod[hat_target] &= ~MOD_BREVE;
                if (lower32(word_buffer[hat_target]) == U'o') char_mod[hat_target] &= ~MOD_HOOK;
                char_mod[hat_target] |= MOD_HAT;
                last_mod_key    = U'6';
                last_mod_target = hat_target;
            }
            return true;
        }
        return false;
    }

    // ── 7 → hook/horn (u/o → ư/ơ): rightmost u/o + cluster extension ────────
    if (keycode == U'7') {
        if (last_mod_key == U'7' && last_mod_target >= 0) {
            const char32_t target_char = lower32(word_buffer[last_mod_target]);
            if (target_char == U'u' || target_char == U'o') {
                for (size_t i = 0; i < buffer_length; ++i) {
                    const char32_t l = lower32(word_buffer[i]);
                    if (l == U'u' || l == U'o') char_mod[i] &= ~MOD_HOOK;
                }
                last_mod_key    = U'\0';
                last_mod_target = -1;
                if (buffer_length < 32) word_buffer[buffer_length++] = keycode;
                return true;
            }
        }

        int hook_target = -1;
        for (int i = static_cast<int>(buffer_length) - 1; i >= 0; --i) {
            const char32_t l = lower32(word_buffer[i]);
            if (l == U'u' || l == U'o') { 
                if (l == U'u' && i >= 2 && lower32(word_buffer[i - 1]) == U'o' && lower32(word_buffer[i - 2]) == U'u') {
                    continue;
                }
                hook_target = i; 
                break; 
            }
        }
        if (hook_target >= 0) {
            const char32_t target_l = lower32(word_buffer[hook_target]);
            int left_idx = hook_target - 1;
            char32_t left_l = (left_idx >= 0) ? lower32(word_buffer[left_idx]) : U'\0';

            if ((target_l == U'o' && left_l == U'u') || 
                (target_l == U'u' && left_l == U'o')) {
                char_mod[hook_target] |= MOD_HOOK;
                char_mod[hook_target] &= ~MOD_HAT;
                char_mod[left_idx] |= MOD_HOOK;
                char_mod[left_idx] &= ~MOD_HAT;
                last_mod_target = hook_target;
            } else if (target_l == U'u' && left_l == U'u') {
                char_mod[left_idx] |= MOD_HOOK;
                char_mod[left_idx] &= ~MOD_HAT;
                last_mod_target = left_idx;
            } else {
                char_mod[hook_target] |= MOD_HOOK;
                char_mod[hook_target] &= ~MOD_HAT;
                last_mod_target = hook_target;
            }
            last_mod_key = U'7';
            return true;
        }
        return false;
    }

    // ── 8 → breve (a → ă) ────────────────────────────────────────────────────
    if (keycode == U'8') {
        int breve_target = -1;
        for (int i = static_cast<int>(buffer_length) - 1; i >= 0; --i) {
            if (lower32(word_buffer[i]) == U'a') { breve_target = i; break; }
        }
        if (breve_target >= 0) {
            if ((char_mod[breve_target] & MOD_BREVE) &&
                last_mod_key == U'8' && last_mod_target == breve_target) {
                for (size_t i = 0; i < buffer_length; ++i) {
                    if (lower32(word_buffer[i]) == U'a') char_mod[i] &= ~MOD_BREVE;
                }
                last_mod_key    = U'\0';
                last_mod_target = -1;
                if (buffer_length < 32) word_buffer[buffer_length++] = keycode;
            } else {
                char_mod[breve_target] |= MOD_BREVE;
                char_mod[breve_target] &= ~MOD_HAT;
                last_mod_key    = U'8';
                last_mod_target = breve_target;
            }
            return true;
        }
        return false;
    }

    // ── 9 → d-slash (d → đ) ──────────────────────────────────────────────────
    if (keycode == U'9') {
        int d_target = -1;
        for (int i = static_cast<int>(buffer_length) - 1; i >= 0; --i) {
            if (lower32(word_buffer[i]) == U'd') { d_target = i; break; }
        }
        if (d_target >= 0) {
            if ((char_mod[d_target] & MOD_D) &&
                last_mod_key == U'9' && last_mod_target == d_target) {
                char_mod[d_target] &= ~MOD_D;
                last_mod_key    = U'\0';
                last_mod_target = -1;
                if (buffer_length < 32) word_buffer[buffer_length++] = keycode;
            } else {
                char_mod[d_target] |= MOD_D;
                last_mod_key    = U'9';
                last_mod_target = d_target;
            }
            return true;
        }
        return false;
    }

    return false;
}

// ──────────────────────────────────────────────────────────────────────────────
//  Main entry point — one call per key-press event, zero allocation
// ──────────────────────────────────────────────────────────────────────────────
bool ShadowEngine::process_key(char32_t keycode, uint8_t mode,
                                char32_t* output_str, size_t& out_len) {

    // ── 1. Smart Backspace ───────────────────────────────────────────────────
    if (__builtin_expect(keycode == 8 || keycode == 127, 0)) {
        if (buffer_length == 0 && current_tone == TONE_NONE) {
            out_len = 0;
            return false;   // pass BackSpace through
        }

        if (current_tone != TONE_NONE) {
            current_tone  = TONE_NONE;
            last_tone_key = U'\0';
        } else {
            const int del = static_cast<int>(buffer_length) - 1;
            buffer_length--;
            char_mod[del] = 0u;

            if (last_mod_target == del) {
                last_mod_target = -1;
                last_mod_key    = U'\0';
            }
        }

        reconstruct_word(output_str, out_len);
        return true;
    }

    // ── 2. Non-printable control keys → reset, do not consume ───────────────
    if (__builtin_expect(keycode < 32, 0)) {
        reset_state();
        out_len = 0;
        return false;
    }

    // ── 3. Word-break detection ──────────────────────────────────────────────
    if (is_word_break(keycode, mode)) {
        reconstruct_word(output_str, out_len);
        reset_state();
        return false;
    }

    // ── 4. Lookbehind Interception for Consonants (e.g. initial 'd' to 'đ') ──
    const char32_t lower_key = lower32(keycode);
    bool consumed = apply_lookbehind_consonant(keycode, lower_key, mode);

    // ── 5. Vowel Tone & Modifiers ────────────────────────────────────────────
    if (!consumed) {
        bool has_vowel = false;
        for (size_t i = 0; i < buffer_length; ++i) {
            if (is_vowel(word_buffer[i])) { has_vowel = true; break; }
        }
        consumed = apply_vowel_tone(keycode, lower_key, mode, has_vowel);
    }

    // ── 6. Buffer capacity guard ─────────────────────────────────────────────
    if (__builtin_expect(!consumed && buffer_length >= 32, 0)) {
        reconstruct_word(output_str, out_len);
        reset_state();
        return false;
    }

    // ── 7. Issue 12 FIX: VNI modifier digits (6-9) with no eligible target ───
    if (!consumed && mode == 1) {
        const char c = static_cast<char>(keycode);
        if (c >= '6' && c <= '9') {
            reconstruct_word(output_str, out_len);
            reset_state();
            return false;
        }
    }

    // ── 8. Raw character push (not consumed by any rule) ─────────────────────
    if (!consumed) {
        word_buffer[buffer_length] = keycode;
        char_mod[buffer_length]    = 0u;
        buffer_length++;
    }

    // ── 9. Reconstruct and return updated composition ────────────────────────
    reconstruct_word(output_str, out_len);
    return true;
}
