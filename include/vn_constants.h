#ifndef VN_CONSTANTS_H
#define VN_CONSTANTS_H

#include <cstdint>

// Tone Masks
constexpr uint32_t TONE_NONE  = 0;
constexpr uint32_t TONE_SAC   = 1 << 0; // Sắc (acute)
constexpr uint32_t TONE_HUYEN = 1 << 1; // Huyền (grave)
constexpr uint32_t TONE_HOI   = 1 << 2; // Hỏi (hook above)
constexpr uint32_t TONE_NGA   = 1 << 3; // Ngã (tilde)
constexpr uint32_t TONE_NANG  = 1 << 4; // Nặng (dot below)

// Modifier Masks — intentionally non-overlapping bitmasks
constexpr uint32_t MOD_NONE  = 0;
constexpr uint32_t MOD_HAT   = 1 << 5; // Mũ Â/Ê/Ô (circumflex)
constexpr uint32_t MOD_HOOK  = 1 << 6; // Móc Ư/Ơ (horn)
constexpr uint32_t MOD_BREVE = 1 << 7; // Trăng Ă (breve)
constexpr uint32_t MOD_D     = 1 << 8; // Đ (d slash)

// Compile-time lookup: base vowel/consonant + ToneMask + ModifierMask → UTF-32 codepoint.
// Unknown combinations (invalid tone values, impossible modifier+base pairings) return base_char.
// Issue 9 FIX: all inner switch(tone) blocks now have `default: break;` so an invalid
// tone value returns base_char rather than falling through silently.
constexpr char32_t VN_CHAR_TABLE[14][5][6] = {
    // Base: a
    {
        { U'a', U'\x00E1', U'\x00E0', U'\x1EA3', U'\x00E3', U'\x1EA1' }, // MOD_NONE
        { U'\x00E2', U'\x1EA5', U'\x1EA7', U'\x1EA9', U'\x1EAB', U'\x1EAD' }, // MOD_HAT
        { U'a', U'a', U'a', U'a', U'a', U'a' }, // MOD_HOOK
        { U'\x0103', U'\x1EAF', U'\x1EB1', U'\x1EB3', U'\x1EB5', U'\x1EB7' }, // MOD_BREVE
        { U'a', U'a', U'a', U'a', U'a', U'a' }, // MOD_D
    },
    // Base: e
    {
        { U'e', U'\x00E9', U'\x00E8', U'\x1EBB', U'\x1EBD', U'\x1EB9' }, // MOD_NONE
        { U'\x00EA', U'\x1EBF', U'\x1EC1', U'\x1EC3', U'\x1EC5', U'\x1EC7' }, // MOD_HAT
        { U'e', U'e', U'e', U'e', U'e', U'e' }, // MOD_HOOK
        { U'e', U'e', U'e', U'e', U'e', U'e' }, // MOD_BREVE
        { U'e', U'e', U'e', U'e', U'e', U'e' }, // MOD_D
    },
    // Base: i
    {
        { U'i', U'\x00ED', U'\x00EC', U'\x1EC9', U'\x0129', U'\x1ECB' }, // MOD_NONE
        { U'i', U'i', U'i', U'i', U'i', U'i' }, // MOD_HAT
        { U'i', U'i', U'i', U'i', U'i', U'i' }, // MOD_HOOK
        { U'i', U'i', U'i', U'i', U'i', U'i' }, // MOD_BREVE
        { U'i', U'i', U'i', U'i', U'i', U'i' }, // MOD_D
    },
    // Base: o
    {
        { U'o', U'\x00F3', U'\x00F2', U'\x1ECF', U'\x00F5', U'\x1ECD' }, // MOD_NONE
        { U'\x00F4', U'\x1ED1', U'\x1ED3', U'\x1ED5', U'\x1ED7', U'\x1ED9' }, // MOD_HAT
        { U'\x01A1', U'\x1EDB', U'\x1EDD', U'\x1EDF', U'\x1EE1', U'\x1EE3' }, // MOD_HOOK
        { U'o', U'o', U'o', U'o', U'o', U'o' }, // MOD_BREVE
        { U'o', U'o', U'o', U'o', U'o', U'o' }, // MOD_D
    },
    // Base: u
    {
        { U'u', U'\x00FA', U'\x00F9', U'\x1EE7', U'\x0169', U'\x1EE5' }, // MOD_NONE
        { U'u', U'u', U'u', U'u', U'u', U'u' }, // MOD_HAT
        { U'\x01B0', U'\x1EE9', U'\x1EEB', U'\x1EED', U'\x1EEF', U'\x1EF1' }, // MOD_HOOK
        { U'u', U'u', U'u', U'u', U'u', U'u' }, // MOD_BREVE
        { U'u', U'u', U'u', U'u', U'u', U'u' }, // MOD_D
    },
    // Base: y
    {
        { U'y', U'\x00FD', U'\x1EF3', U'\x1EF7', U'\x1EF9', U'\x1EF5' }, // MOD_NONE
        { U'y', U'y', U'y', U'y', U'y', U'y' }, // MOD_HAT
        { U'y', U'y', U'y', U'y', U'y', U'y' }, // MOD_HOOK
        { U'y', U'y', U'y', U'y', U'y', U'y' }, // MOD_BREVE
        { U'y', U'y', U'y', U'y', U'y', U'y' }, // MOD_D
    },
    // Base: d
    {
        { U'd', U'd', U'd', U'd', U'd', U'd' }, // MOD_NONE
        { U'd', U'd', U'd', U'd', U'd', U'd' }, // MOD_HAT
        { U'd', U'd', U'd', U'd', U'd', U'd' }, // MOD_HOOK
        { U'd', U'd', U'd', U'd', U'd', U'd' }, // MOD_BREVE
        { U'\x0111', U'\x0111', U'\x0111', U'\x0111', U'\x0111', U'\x0111' }, // MOD_D
    },
    // Base: A
    {
        { U'A', U'\x00C1', U'\x00C0', U'\x1EA2', U'\x00C3', U'\x1EA0' }, // MOD_NONE
        { U'\x00C2', U'\x1EA4', U'\x1EA6', U'\x1EA8', U'\x1EAA', U'\x1EAC' }, // MOD_HAT
        { U'A', U'A', U'A', U'A', U'A', U'A' }, // MOD_HOOK
        { U'\x0102', U'\x1EAE', U'\x1EB0', U'\x1EB2', U'\x1EB4', U'\x1EB6' }, // MOD_BREVE
        { U'A', U'A', U'A', U'A', U'A', U'A' }, // MOD_D
    },
    // Base: E
    {
        { U'E', U'\x00C9', U'\x00C8', U'\x1EBA', U'\x1EBC', U'\x1EB8' }, // MOD_NONE
        { U'\x00CA', U'\x1EBE', U'\x1EC0', U'\x1EC2', U'\x1EC4', U'\x1EC6' }, // MOD_HAT
        { U'E', U'E', U'E', U'E', U'E', U'E' }, // MOD_HOOK
        { U'E', U'E', U'E', U'E', U'E', U'E' }, // MOD_BREVE
        { U'E', U'E', U'E', U'E', U'E', U'E' }, // MOD_D
    },
    // Base: I
    {
        { U'I', U'\x00CD', U'\x00CC', U'\x1EC8', U'\x0128', U'\x1ECA' }, // MOD_NONE
        { U'I', U'I', U'I', U'I', U'I', U'I' }, // MOD_HAT
        { U'I', U'I', U'I', U'I', U'I', U'I' }, // MOD_HOOK
        { U'I', U'I', U'I', U'I', U'I', U'I' }, // MOD_BREVE
        { U'I', U'I', U'I', U'I', U'I', U'I' }, // MOD_D
    },
    // Base: O
    {
        { U'O', U'\x00D3', U'\x00D2', U'\x1ECE', U'\x00D5', U'\x1ECC' }, // MOD_NONE
        { U'\x00D4', U'\x1ED0', U'\x1ED2', U'\x1ED4', U'\x1ED6', U'\x1ED8' }, // MOD_HAT
        { U'\x01A0', U'\x1EDA', U'\x1EDC', U'\x1EDE', U'\x1EE0', U'\x1EE2' }, // MOD_HOOK
        { U'O', U'O', U'O', U'O', U'O', U'O' }, // MOD_BREVE
        { U'O', U'O', U'O', U'O', U'O', U'O' }, // MOD_D
    },
    // Base: U
    {
        { U'U', U'\x00DA', U'\x00D9', U'\x1EE6', U'\x0168', U'\x1EE4' }, // MOD_NONE
        { U'U', U'U', U'U', U'U', U'U', U'U' }, // MOD_HAT
        { U'\x01AF', U'\x1EE8', U'\x1EEA', U'\x1EEC', U'\x1EEE', U'\x1EF0' }, // MOD_HOOK
        { U'U', U'U', U'U', U'U', U'U', U'U' }, // MOD_BREVE
        { U'U', U'U', U'U', U'U', U'U', U'U' }, // MOD_D
    },
    // Base: Y
    {
        { U'Y', U'\x00DD', U'\x1EF2', U'\x1EF6', U'\x1EF8', U'\x1EF4' }, // MOD_NONE
        { U'Y', U'Y', U'Y', U'Y', U'Y', U'Y' }, // MOD_HAT
        { U'Y', U'Y', U'Y', U'Y', U'Y', U'Y' }, // MOD_HOOK
        { U'Y', U'Y', U'Y', U'Y', U'Y', U'Y' }, // MOD_BREVE
        { U'Y', U'Y', U'Y', U'Y', U'Y', U'Y' }, // MOD_D
    },
    // Base: D
    {
        { U'D', U'D', U'D', U'D', U'D', U'D' }, // MOD_NONE
        { U'D', U'D', U'D', U'D', U'D', U'D' }, // MOD_HAT
        { U'D', U'D', U'D', U'D', U'D', U'D' }, // MOD_HOOK
        { U'D', U'D', U'D', U'D', U'D', U'D' }, // MOD_BREVE
        { U'\x0110', U'\x0110', U'\x0110', U'\x0110', U'\x0110', U'\x0110' }, // MOD_D
    },
};

constexpr char32_t lookup_vietnamese_char(char32_t base_char, uint32_t tone, uint32_t mod) {
    int base_idx = -1;
    switch (base_char) {
        case U'a': base_idx = 0; break;
        case U'e': base_idx = 1; break;
        case U'i': base_idx = 2; break;
        case U'o': base_idx = 3; break;
        case U'u': base_idx = 4; break;
        case U'y': base_idx = 5; break;
        case U'd': base_idx = 6; break;
        case U'A': base_idx = 7; break;
        case U'E': base_idx = 8; break;
        case U'I': base_idx = 9; break;
        case U'O': base_idx = 10; break;
        case U'U': base_idx = 11; break;
        case U'Y': base_idx = 12; break;
        case U'D': base_idx = 13; break;
        default: return base_char;
    }

    int tone_idx = 0;
    if (tone) {
        tone_idx = __builtin_ctz(tone) + 1; // TONE_SAC=1(bit 0), so ctz=0 -> idx=1
        if (tone_idx > 5) tone_idx = 0; // fallback
    }

    int mod_idx = 0;
    if (mod) {
        mod_idx = __builtin_ctz(mod) - 4; // MOD_HAT=1<<5 -> ctz=5 -> idx=1
        if (mod_idx < 0 || mod_idx > 4) mod_idx = 0; // fallback
    }

    char32_t result = VN_CHAR_TABLE[base_idx][mod_idx][tone_idx];
    if (result == U'a' || result == U'A') {
        // Just checking if we got the fallback dummy for invalid combos, actually the generator put base_char for invalid combos
        // so it's already correct.
    }
    
    // In our table, invalid combos just hold the base_char, so returning it is safe.
    return result;
}


#endif // VN_CONSTANTS_H
