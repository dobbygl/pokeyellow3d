#include "hud_text.h"
#include "kanto_rom.h"
#include "rom_text_region.h"
#include <cstdio>
#include <cstdlib>

static void require(bool ok, const char *message) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
int main() {
    require(hud_text::encode("Az09 \xc3\xa9'!?./,") ==
                std::vector<uint8_t>(
                    {0x80, 0xb9, 0xf6, 0xff, 0x7f, 0xba, 0xe0, 0xe7, 0xe6, 0xe8, 0xf3, 0xf4}),
            "HUD encoding preserves the verified English ROM charmap");
    require(hud_text::encode("Pok\xc3\xa9mon").size() == 7, "UTF-8 e acute is one original glyph");
    require(hud_text::encode("unsupported_").empty(), "unknown glyph rejects the whole label");
    for (int map = 0; map < kanto::MapCount; ++map)
        require(!hud_text::encode(kanto::title(map)).empty(),
                "every existing map title fits the font");
    const uint8_t name[]{0x8d, 0x88, 0x83, 0x8e, 0x91, 0x80, 0x8d, 0xf5, 0x50, 0};
    require(hud_text::supported(name, sizeof name) && hud_text::length(name, sizeof name) == 8,
            "raw names retain gender glyphs and stop at the original terminator");
    const uint8_t extra[]{0x80, 0x6e, 0x50};
    require(!hud_text::supported(extra, sizeof extra),
            "extra-font names require original fallback");
    std::vector<uint8_t> rom(rom_font::Offset + rom_font::Glyphs * 8, 0);
    for (int y = 0; y < 8; ++y)
        rom[rom_font::Offset + y] = uint8_t(0x80 >> y);
    auto font = rom_font::decode(rom.data(), rom.size());
    std::array<uint8_t, 8192> vram{};
    for (int y = 0; y < 8; ++y)
        vram[0x800 + 2 * y] = vram[0x800 + 2 * y + 1] = uint8_t(0x80 >> y);
    std::array<uint8_t, 360> tiles{};
    tiles.fill(0x7f);
    tiles[21] = 0x80;
    std::array<uint32_t, 160 * 144> lcd{};
    lcd.fill(0xffffff);
    for (int y = 0; y < 8; ++y)
        lcd[(8 + y) * 160 + 8 + y] = 0;
    auto ready = [&] {
        return rom_text_region::ready(tiles.data(), vram.data(), font, lcd.data(), {1, 1, 2, 1});
    };
    require(ready(), "only displayed, matching ROM text can be restyled");
    tiles[21] = 0x81;
    require(!ready(), "pending wTileMap text must retain the displayed LCD");
    tiles[21] = 0x80;
    vram[0x800] ^= 1;
    require(!ready(), "replaced VRAM glyph falls back for the entire region");
    vram[0x800] ^= 1;
    tiles[22] = 0x6e;
    require(!ready(), "special glyph in an otherwise supported label keeps original pixels");
    tiles[22] = 0x7f;
    lcd.fill(0xffffff);
    require(!ready(), "white transition never invents a not-yet-displayed glyph");
    std::puts("PASS: original HUD charmap, all map titles, raw names and region fallback timing");
}
