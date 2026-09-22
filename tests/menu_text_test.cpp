#include "menu_text.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

static void require(bool ok, const char *message) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
int main(int argc, char **argv) {
    std::vector<uint8_t> rom(rom_font::Offset + rom_font::Glyphs * 8);
    for (size_t i = rom_font::Offset; i < rom.size(); ++i)
        rom[i] = uint8_t(i * 37);
    auto font = rom_font::decode(rom.data(), rom.size());
    std::array<uint8_t, 8192> vram{};
    for (int glyph = 0; glyph < 128; ++glyph)
        for (int y = 0; y < 8; ++y)
            for (int p = 0; p < 2; ++p)
                vram[0x800 + glyph * 16 + y * 2 + p] = font.rows[glyph * 8 + y];
    auto check = [&](const std::array<uint8_t, 360> &tiles, const menu_layout::Layout &layout) {
        auto plan = menu_text::prepare(tiles.data(), vram.data(), font, layout, 800, 720, 14, 3, 4);
        require(plan.count == layout.count, "all partial regions retained");
        for (int y = 0; y < 18; ++y)
            for (int x = 0; x < 20; ++x) {
                int expected = -1;
                for (size_t i = 0; i < layout.count; ++i)
                    if (menu_layout::contains(layout.regions[i], x, y))
                        expected = int(i);
                require(plan.owner[y * 20 + x] == expected, "topmost source cell appears once");
            }
        for (size_t i = 0; i < plan.count; ++i) {
            auto &p = plan.panels[i];
            require(p.scale == (menu_text::is_bottom(layout.regions[i]) ? 4 : 3),
                    "default integer glyph scales");
            require(!p.visible || (p.left >= 0 && p.top >= 0 && p.right <= 800 && p.bottom <= 720),
                    "complete panel inside viewport");
        }
        return plan;
    };
    using menu_layout::Bottom;
    menu_layout::Layout layout;
    layout.kind = menu_layout::Kind::Partial;
    layout.regions[0] = {10, 0, 10, 16};
    layout.regions[1] = Bottom;
    layout.count = 2;
    std::array<uint8_t, 360> tiles{};
    for (size_t i = 0; i < layout.count; ++i) {
        auto r = layout.regions[i];
        for (int y = r.y; y < r.y + r.h; ++y)
            for (int x = r.x; x < r.x + r.w; ++x) {
                auto border = menu_layout::border(r, x, y);
                tiles[y * 20 + x] = border ? border : 0x7f;
            }
    }
    tiles[2 * 20 + 11] = menu_text::Cursor;
    tiles[13 * 20 + 1] = 0xba;
    auto plan = check(tiles, layout);
    require(!plan.panels[0].fallback && !plan.panels[1].fallback, "font tiles are integrated");
    require(plan.panels[0].left == 566 && plan.panels[0].right == 786,
            "Start has threefold glyphs and 14px padding");
    require(plan.panels[1].left == 98 && plan.panels[1].right == 702,
            "dialogue has fourfold glyphs and 14px padding");
    tiles[13 * 20 + 1] = 0x6e;
    plan = check(tiles, layout);
    require(!plan.panels[0].fallback && plan.panels[1].fallback &&
                plan.panels[1].rejected_tile == 0x6e,
            "extra font falls back only in its own region");
    tiles[13 * 20 + 1] = 0xba;
    vram[0x800 + (0xba - 128) * 16] ^= 1;
    plan = check(tiles, layout);
    require(!plan.panels[0].fallback && plan.panels[1].fallback,
            "same tile index with replaced graphics retains LCD");
    vram[0x800 + (0xba - 128) * 16] ^= 1;
    require(!menu_text::prepare(nullptr, vram.data(), font, layout, 800, 720, 14, 3, 4).count,
            "missing tilemap cannot synthesize a panel");
    // Full-screen placement shares the source ownership, with a uniform grid.
    // Exceptions are two specifically verified ROM graphics, never arbitrary
    // tiles admitted just because a PC is open.
    rom.resize(1048576);
    for (uint8_t tile : {menu_graphics::Level, menu_graphics::OccupiedBox}) {
        size_t source = menu_graphics::offset(tile);
        for (int i = 0; i < 16; ++i)
            rom[source + i] = vram[0x1000 + tile * 16 + i] = uint8_t(13 * i + tile);
        tiles[13 * 20 + 1] = tile;
        plan = menu_text::prepare(tiles.data(), vram.data(), font, layout, 800, 720, 14, 3, 4,
                                  menu_text::Placement::Full, rom.data(), rom.size());
        require(!plan.panels[1].fallback && plan.panels[1].scale == 3 &&
                    plan.panels[1].origin_x == 160 && plan.panels[1].origin_y == 144,
                "complete screen uses a centered uniform integer grid");
        require(check(tiles, layout).panels[1].fallback,
                "special graphics do not change legacy partial-screen validation");
        vram[0x1000 + tile * 16 + 7] ^= 1;
        plan = menu_text::prepare(tiles.data(), vram.data(), font, layout, 800, 720, 14, 3, 4,
                                  menu_text::Placement::Full, rom.data(), rom.size());
        require(plan.panels[1].fallback, "changed graphic bitplane rejects the full-screen plan");
        vram[0x1000 + tile * 16 + 7] ^= 1;
    }
    tiles[13 * 20 + 1] = 0x6d;
    plan = menu_text::prepare(tiles.data(), vram.data(), font, layout, 800, 720, 14, 3, 4,
                              menu_text::Placement::Full, rom.data(), rom.size());
    require(plan.panels[1].fallback, "unverified extra font tile remains unsupported");
    // Real private shop/save fixtures exercise every original overlap. Graphics
    // are synthetic here; the GL observer independently checks the actual ROM.
    for (int i = 1; i < argc; ++i) {
        FILE *f = std::fopen(argv[i], "rb");
        require(f, "private tilemap opens");
        require(std::fread(tiles.data(), 1, 360, f) == 360 && std::fgetc(f) == EOF,
                "private tilemap size");
        std::fclose(f);
        layout = menu_layout::classify(tiles.data());
        if (layout.kind == menu_layout::Kind::Partial)
            check(tiles, layout);
        else
            require(!menu_text::prepare(tiles.data(), vram.data(), font, layout, 800, 720, 14, 3, 4)
                         .count,
                    "unknown/full screen uses existing compositor");
    }
    std::puts("PASS: ownership, scales, padding, regional fallback and private layouts");
}
