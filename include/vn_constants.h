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
constexpr char32_t lookup_vietnamese_char(char32_t base_char, uint32_t tone, uint32_t mod) {
    switch (base_char) {
        case U'a':
            if (mod & MOD_HAT) {
                switch (tone) {
                    case TONE_NONE:  return U'â'; // 0x00E2
                    case TONE_SAC:   return U'ấ'; // 0x1EA5
                    case TONE_HUYEN: return U'ầ'; // 0x1EA7
                    case TONE_HOI:   return U'ẩ'; // 0x1EA9
                    case TONE_NGA:   return U'ẫ'; // 0x1EAB
                    case TONE_NANG:  return U'ậ'; // 0x1EAD
                    default: break;
                }
            } else if (mod & MOD_BREVE) {
                switch (tone) {
                    case TONE_NONE:  return U'ă'; // 0x0103
                    case TONE_SAC:   return U'ắ'; // 0x1EAF
                    case TONE_HUYEN: return U'ằ'; // 0x1EB1
                    case TONE_HOI:   return U'ẳ'; // 0x1EB3
                    case TONE_NGA:   return U'ẵ'; // 0x1EB5
                    case TONE_NANG:  return U'ặ'; // 0x1EB7
                    default: break;
                }
            } else {
                switch (tone) {
                    case TONE_NONE:  return U'a'; // 0x0061
                    case TONE_SAC:   return U'á'; // 0x00E1
                    case TONE_HUYEN: return U'à'; // 0x00E0
                    case TONE_HOI:   return U'ả'; // 0x1EA3
                    case TONE_NGA:   return U'ã'; // 0x00E3
                    case TONE_NANG:  return U'ạ'; // 0x1EA1
                    default: break;
                }
            }
            break;
        case U'A':
            if (mod & MOD_HAT) {
                switch (tone) {
                    case TONE_NONE:  return U'Â'; // 0x00C2
                    case TONE_SAC:   return U'Ấ'; // 0x1EA4
                    case TONE_HUYEN: return U'Ầ'; // 0x1EA6
                    case TONE_HOI:   return U'Ẩ'; // 0x1EA8
                    case TONE_NGA:   return U'Ẫ'; // 0x1EAA
                    case TONE_NANG:  return U'Ậ'; // 0x1EAC
                    default: break;
                }
            } else if (mod & MOD_BREVE) {
                switch (tone) {
                    case TONE_NONE:  return U'Ă'; // 0x0102
                    case TONE_SAC:   return U'Ắ'; // 0x1EAE
                    case TONE_HUYEN: return U'Ằ'; // 0x1EB0
                    case TONE_HOI:   return U'Ẳ'; // 0x1EB2
                    case TONE_NGA:   return U'Ẵ'; // 0x1EB4
                    case TONE_NANG:  return U'Ặ'; // 0x1EB6
                    default: break;
                }
            } else {
                switch (tone) {
                    case TONE_NONE:  return U'A'; // 0x0041
                    case TONE_SAC:   return U'Á'; // 0x00C1
                    case TONE_HUYEN: return U'À'; // 0x00C0
                    case TONE_HOI:   return U'Ả'; // 0x1EA2
                    case TONE_NGA:   return U'Ã'; // 0x00C3
                    case TONE_NANG:  return U'Ạ'; // 0x1EA0
                    default: break;
                }
            }
            break;
        case U'e':
            if (mod & MOD_HAT) {
                switch (tone) {
                    case TONE_NONE:  return U'ê'; // 0x00EA
                    case TONE_SAC:   return U'ế'; // 0x1EBF
                    case TONE_HUYEN: return U'ề'; // 0x1EC1
                    case TONE_HOI:   return U'ể'; // 0x1EC3
                    case TONE_NGA:   return U'ễ'; // 0x1EC5
                    case TONE_NANG:  return U'ệ'; // 0x1EC7
                    default: break;
                }
            } else {
                switch (tone) {
                    case TONE_NONE:  return U'e'; // 0x0065
                    case TONE_SAC:   return U'é'; // 0x00E9
                    case TONE_HUYEN: return U'è'; // 0x00E8
                    case TONE_HOI:   return U'ẻ'; // 0x1EBB
                    case TONE_NGA:   return U'ẽ'; // 0x1EBD
                    case TONE_NANG:  return U'ẹ'; // 0x1EB9
                    default: break;
                }
            }
            break;
        case U'E':
            if (mod & MOD_HAT) {
                switch (tone) {
                    case TONE_NONE:  return U'Ê'; // 0x00CA
                    case TONE_SAC:   return U'Ế'; // 0x1EBE
                    case TONE_HUYEN: return U'Ề'; // 0x1EC0
                    case TONE_HOI:   return U'Ể'; // 0x1EC2
                    case TONE_NGA:   return U'Ễ'; // 0x1EC4
                    case TONE_NANG:  return U'Ệ'; // 0x1EC6
                    default: break;
                }
            } else {
                switch (tone) {
                    case TONE_NONE:  return U'E'; // 0x0045
                    case TONE_SAC:   return U'É'; // 0x00C9
                    case TONE_HUYEN: return U'È'; // 0x00C8
                    case TONE_HOI:   return U'Ẻ'; // 0x1EBA
                    case TONE_NGA:   return U'Ẽ'; // 0x1EBC
                    case TONE_NANG:  return U'Ẹ'; // 0x1EB8
                    default: break;
                }
            }
            break;
        case U'o':
            if (mod & MOD_HAT) {
                switch (tone) {
                    case TONE_NONE:  return U'ô'; // 0x00F4
                    case TONE_SAC:   return U'ố'; // 0x1ED1
                    case TONE_HUYEN: return U'ồ'; // 0x1ED3
                    case TONE_HOI:   return U'ổ'; // 0x1ED5
                    case TONE_NGA:   return U'ỗ'; // 0x1ED7
                    case TONE_NANG:  return U'ộ'; // 0x1ED9
                    default: break;
                }
            } else if (mod & MOD_HOOK) {
                switch (tone) {
                    case TONE_NONE:  return U'ơ'; // 0x01A1
                    case TONE_SAC:   return U'ớ'; // 0x1EDB
                    case TONE_HUYEN: return U'ờ'; // 0x1EDD
                    case TONE_HOI:   return U'ở'; // 0x1EDF
                    case TONE_NGA:   return U'ỡ'; // 0x1EE1
                    case TONE_NANG:  return U'ợ'; // 0x1EE3
                    default: break;
                }
            } else {
                switch (tone) {
                    case TONE_NONE:  return U'o'; // 0x006F
                    case TONE_SAC:   return U'ó'; // 0x00F3
                    case TONE_HUYEN: return U'ò'; // 0x00F2
                    case TONE_HOI:   return U'ỏ'; // 0x1ECF
                    case TONE_NGA:   return U'õ'; // 0x00F5
                    case TONE_NANG:  return U'ọ'; // 0x1ECD
                    default: break;
                }
            }
            break;
        case U'O':
            if (mod & MOD_HAT) {
                switch (tone) {
                    case TONE_NONE:  return U'Ô'; // 0x00D4
                    case TONE_SAC:   return U'Ố'; // 0x1ED0
                    case TONE_HUYEN: return U'Ồ'; // 0x1ED2
                    case TONE_HOI:   return U'Ổ'; // 0x1ED4
                    case TONE_NGA:   return U'Ỗ'; // 0x1ED6
                    case TONE_NANG:  return U'Ộ'; // 0x1ED8
                    default: break;
                }
            } else if (mod & MOD_HOOK) {
                switch (tone) {
                    case TONE_NONE:  return U'Ơ'; // 0x01A0
                    case TONE_SAC:   return U'Ớ'; // 0x1EDA
                    case TONE_HUYEN: return U'Ờ'; // 0x1EDC
                    case TONE_HOI:   return U'Ở'; // 0x1EDE
                    case TONE_NGA:   return U'Ỡ'; // 0x1EE0
                    case TONE_NANG:  return U'Ợ'; // 0x1EE2
                    default: break;
                }
            } else {
                switch (tone) {
                    case TONE_NONE:  return U'O'; // 0x004F
                    case TONE_SAC:   return U'Ó'; // 0x00D3
                    case TONE_HUYEN: return U'Ò'; // 0x00D2
                    case TONE_HOI:   return U'Ỏ'; // 0x1ECE
                    case TONE_NGA:   return U'Õ'; // 0x00D5
                    case TONE_NANG:  return U'Ọ'; // 0x1ECC
                    default: break;
                }
            }
            break;
        case U'u':
            if (mod & MOD_HOOK) {
                switch (tone) {
                    case TONE_NONE:  return U'ư'; // 0x01B0
                    case TONE_SAC:   return U'ứ'; // 0x1EE9
                    case TONE_HUYEN: return U'ừ'; // 0x1EEB
                    case TONE_HOI:   return U'ử'; // 0x1EED
                    case TONE_NGA:   return U'ữ'; // 0x1EEF
                    case TONE_NANG:  return U'ự'; // 0x1EF1
                    default: break;
                }
            } else {
                switch (tone) {
                    case TONE_NONE:  return U'u'; // 0x0075
                    case TONE_SAC:   return U'ú'; // 0x00FA
                    case TONE_HUYEN: return U'ù'; // 0x00F9
                    case TONE_HOI:   return U'ủ'; // 0x1EE7
                    case TONE_NGA:   return U'ũ'; // 0x0169
                    case TONE_NANG:  return U'ụ'; // 0x1EE5
                    default: break;
                }
            }
            break;
        case U'U':
            if (mod & MOD_HOOK) {
                switch (tone) {
                    case TONE_NONE:  return U'Ư'; // 0x01AF
                    case TONE_SAC:   return U'Ứ'; // 0x1EE8
                    case TONE_HUYEN: return U'Ừ'; // 0x1EEA
                    case TONE_HOI:   return U'Ử'; // 0x1EEC
                    case TONE_NGA:   return U'Ữ'; // 0x1EEE
                    case TONE_NANG:  return U'Ự'; // 0x1EF0
                    default: break;
                }
            } else {
                switch (tone) {
                    case TONE_NONE:  return U'U'; // 0x0055
                    case TONE_SAC:   return U'Ú'; // 0x00DA
                    case TONE_HUYEN: return U'Ù'; // 0x00D9
                    case TONE_HOI:   return U'Ủ'; // 0x1EE6
                    case TONE_NGA:   return U'Ũ'; // 0x0168
                    case TONE_NANG:  return U'Ụ'; // 0x1EE4
                    default: break;
                }
            }
            break;
        case U'i':
            switch (tone) {
                case TONE_NONE:  return U'i'; // 0x0069
                case TONE_SAC:   return U'í'; // 0x00ED
                case TONE_HUYEN: return U'ì'; // 0x00EC
                case TONE_HOI:   return U'ỉ'; // 0x1EC9
                case TONE_NGA:   return U'ĩ'; // 0x0129
                case TONE_NANG:  return U'ị'; // 0x1ECB
                default: break;
            }
            break;
        case U'I':
            switch (tone) {
                case TONE_NONE:  return U'I'; // 0x0049
                case TONE_SAC:   return U'Í'; // 0x00CD
                case TONE_HUYEN: return U'Ì'; // 0x00CC
                case TONE_HOI:   return U'Ỉ'; // 0x1EC8
                case TONE_NGA:   return U'Ĩ'; // 0x0128
                case TONE_NANG:  return U'Ị'; // 0x1ECA
                default: break;
            }
            break;
        case U'y':
            switch (tone) {
                case TONE_NONE:  return U'y'; // 0x0079
                case TONE_SAC:   return U'ý'; // 0x00FD
                case TONE_HUYEN: return U'ỳ'; // 0x1EF3
                case TONE_HOI:   return U'ỷ'; // 0x1EF7
                case TONE_NGA:   return U'ỹ'; // 0x1EF9
                case TONE_NANG:  return U'ỵ'; // 0x1EF5
                default: break;
            }
            break;
        case U'Y':
            switch (tone) {
                case TONE_NONE:  return U'Y'; // 0x0059
                case TONE_SAC:   return U'Ý'; // 0x00DD
                case TONE_HUYEN: return U'Ỳ'; // 0x1EF2
                case TONE_HOI:   return U'Ỷ'; // 0x1EF6
                case TONE_NGA:   return U'Ỹ'; // 0x1EF8
                case TONE_NANG:  return U'Ỵ'; // 0x1EF4
                default: break;
            }
            break;
        case U'd':
            if (mod & MOD_D) return U'đ'; // 0x0111
            return U'd';
        case U'D':
            if (mod & MOD_D) return U'Đ'; // 0x0110
            return U'D';
        default: break;
    }
    return base_char;
}

#endif // VN_CONSTANTS_H
