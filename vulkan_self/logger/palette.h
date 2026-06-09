#pragma once
#include <cstdint>
#include <iostream>

namespace LoggerPalette {
    using Color = std::uint32_t;

    // Базовые
    inline constexpr Color white        = 0xF5F5F5; // #F5F5F5
    inline constexpr Color light_gray   = 0xCFCFCF; // #CFCFCF
    inline constexpr Color gray         = 0x9CA3AF; // #9CA3AF
    inline constexpr Color dark_gray    = 0x4B5563; // #4B5563
    inline constexpr Color black        = 0x111111; // #111111

    // Холодные
    inline constexpr Color blue         = 0x60A5FA; // #60A5FA
    inline constexpr Color deep_blue    = 0x2563EB; // #2563EB
    inline constexpr Color cyan         = 0x22D3EE; // #22D3EE
    inline constexpr Color teal         = 0x14B8A6; // #14B8A6
    inline constexpr Color aqua         = 0x67E8F9; // #67E8F9

    // Тёплые
    inline constexpr Color green        = 0x4ADE80; // #4ADE80
    inline constexpr Color lime         = 0xA3E635; // #A3E635
    inline constexpr Color yellow       = 0xFACC15; // #FACC15
    inline constexpr Color orange       = 0xFB923C; // #FB923C
    inline constexpr Color red          = 0xF87171; // #F87171

    // Акцентные
    inline constexpr Color pink         = 0xF472B6; // #F472B6
    inline constexpr Color purple       = 0xC084FC; // #C084FC
    inline constexpr Color violet       = 0x8B5CF6; // #8B5CF6

    // Для уровней логирования
    inline constexpr Color trace        = 0x9CA3AF; // #9CA3AF
    inline constexpr Color debug        = 0x60A5FA; // #60A5FA
    inline constexpr Color info         = 0x22C55E; // #22C55E
    inline constexpr Color success      = 0x4ADE80; // #4ADE80
    inline constexpr Color warning      = 0xFACC15; // #FACC15
    inline constexpr Color error        = 0xF87171; // #F87171
    inline constexpr Color critical     = 0xEF4444; // #EF4444
    inline constexpr Color fatal        = 0xDC2626; // #DC2626

    // Для заголовков / секций
    inline constexpr Color section      = 0xC084FC; // #C084FC
    inline constexpr Color highlight    = 0x67E8F9; // #67E8F9
    inline constexpr Color muted        = 0x94A3B8; // #94A3B8
}
