#pragma once
#include "hud_text.h"
#include "menu_text_gl.h"

namespace hud_text {
inline bool draw(const uint8_t *rom, size_t rom_size, const uint8_t *tiles, size_t count,
                 ImVec2 position, int scale, ImU32 color) {
    if (!supported(tiles, count) || !menu_text::upload(rom_font::get(rom, rom_size)))
        return false;
    const int cell = 8 * std::max(1, scale);
    position.x = std::round(position.x);
    position.y = std::round(position.y);
    auto *dl = ImGui::GetForegroundDrawList();
    for (size_t i = 0; i < count && tiles[i] != 0x50; ++i) {
        if (tiles[i] < 0x80)
            continue;
        int glyph = tiles[i] - 0x80;
        ImVec2 from{position.x + float(i * cell), position.y};
        dl->AddImage((ImTextureID)(intptr_t)menu_text::texture, from,
                     {from.x + cell, from.y + cell},
                     {float(glyph % 16) / 16, float(glyph / 16) / 8},
                     {float(glyph % 16 + 1) / 16, float(glyph / 16 + 1) / 8}, color);
    }
    return true;
}
inline bool label(const uint8_t *rom, size_t rom_size, std::string_view value, ImVec2 position,
                  int scale, ImU32 color) {
    auto tiles = encode(value);
    return (value.empty() || !tiles.empty()) &&
           draw(rom, rom_size, tiles.data(), tiles.size(), position, scale, color);
}
} // namespace hud_text
