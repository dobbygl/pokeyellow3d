#include "battle_menu_layout.h"
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
    std::vector<uint8_t> rom(rom_font::Offset + rom_font::Glyphs * 8, 0xa5);
    auto font = rom_font::decode(rom.data(), rom.size());
    std::array<uint8_t, 8192> vram{};
    std::fill(vram.begin() + 0x800, vram.begin() + 0x1000, 0xa5);
    auto plan = [&](const auto &tiles, const battle_menu::Layout &layout) {
        return menu_text::prepare(tiles.data(), vram.data(), font, layout.regions, 800, 720, 14, 3,
                                  4, menu_text::Placement::Battle);
    };
    auto stamp = [](auto &tiles, menu_layout::Rect r) {
        for (int y = r.y; y < r.y + r.h; ++y)
            for (int x = r.x; x < r.x + r.w; ++x) {
                auto edge = menu_layout::border(r, x, y);
                tiles[y * 20 + x] = edge ? edge : 0x7f;
            }
    };
    std::array<uint8_t, 360> tiles{};
    stamp(tiles, menu_layout::Bottom);
    auto layout = battle_menu::classify(tiles.data());
    require(layout.kind == battle_menu::Kind::Message, "original bottom window");
    require(!plan(tiles, layout).panels[0].visible, "empty battle message has no panel");
    tiles[14 * 20 + 1] = 0x80;
    require(plan(tiles, layout).panels[0].visible, "first original glyph displays the message");
    tiles[14 * 20 + 1] = 0x7f;
    stamp(tiles, battle_menu::Fight);
    tiles[14 * 20 + 9] = 0xed;
    layout = battle_menu::classify(tiles.data());
    require(layout.kind == battle_menu::Kind::Fight, "split fight menu");
    auto p = plan(tiles, layout);
    require(!p.panels[0].visible && p.panels[1].visible, "empty left message is hidden");
    require(p.panels[1].scale == 4 && p.panels[1].top == 550 && p.panels[1].right == 702,
            "fight controls share dialogue scale and bottom alignment");
    stamp(tiles, menu_layout::Bottom);
    stamp(tiles, battle_menu::Moves);
    stamp(tiles, battle_menu::MoveInfo);
    tiles[13 * 20 + 5] = 0xed;
    tiles[9 * 20 + 1] = 0x93;
    layout = battle_menu::classify(tiles.data());
    require(layout.kind == battle_menu::Kind::Moves, "move list and PP/type window");
    p = plan(tiles, layout);
    require(!p.panels[0].visible && p.panels[1].visible && p.panels[2].visible,
            "only the two populated move panels appear");
    require(p.owner[12 * 20 + 4] == 2 && p.panels[2].top == 422 && p.panels[2].bottom == 546,
            "PP/type owns the covered top border of the move list");
    // Four move rows (13-16) below TYPE/PP (rows 9-11): the shared bottom grid
    // leaves only 4px between both panels. separate() lifts TYPE/PP alone.
    for (int row = 13; row <= 16; ++row)
        tiles[row * 20 + 6] = uint8_t(0x86 + row);
    tiles[10 * 20 + 2] = 0x84;
    tiles[11 * 20 + 7] = 0xf8;
    p = plan(tiles, layout);
    require(p.panels[1].top == 550 && p.panels[2].bottom == 546 &&
                p.panels[2].right > p.panels[1].left,
            "unseparated TYPE/PP nearly touches the first of four move rows");
    auto lifted = p;
    menu_text::separate(lifted, battle_menu::MovesInfo, battle_menu::MovesList, 14);
    require(lifted.panels[2].bottom + 14 <= lifted.panels[1].top &&
                lifted.panels[2].bottom == 536 && lifted.panels[2].top == 412 &&
                lifted.panels[2].origin_y == p.panels[2].origin_y - 10 &&
                lifted.panels[2].origin_x == p.panels[2].origin_x &&
                lifted.panels[2].left == p.panels[2].left &&
                lifted.panels[2].right == p.panels[2].right,
            "TYPE/PP and its glyph grid rise by whole pixels to clear the move rows");
    require(lifted.panels[1].origin_y == p.panels[1].origin_y &&
                lifted.panels[1].top == p.panels[1].top &&
                lifted.panels[0].origin_y == p.panels[0].origin_y && lifted.owner == p.owner &&
                lifted.panels[2].scale == p.panels[2].scale,
            "move list, message and source ownership are unchanged");
    auto again = lifted;
    menu_text::separate(again, battle_menu::MovesInfo, battle_menu::MovesList, 14);
    require(again.panels[2].origin_y == lifted.panels[2].origin_y, "separation is idempotent");
    // Controls: nothing moves without a horizontal overlap, a visible pair or
    // valid indices, and the lower panel is never the one displaced.
    auto apart = p;
    apart.panels[2].right = apart.panels[1].left;
    menu_text::separate(apart, battle_menu::MovesInfo, battle_menu::MovesList, 14);
    require(apart.panels[2].origin_y == p.panels[2].origin_y, "side-by-side panels stay put");
    auto hidden = p;
    hidden.panels[1].visible = false;
    menu_text::separate(hidden, battle_menu::MovesInfo, battle_menu::MovesList, 14);
    require(hidden.panels[2].origin_y == p.panels[2].origin_y, "an invisible lower panel");
    auto invalid = p;
    menu_text::separate(invalid, 5, battle_menu::MovesList, 14);
    menu_text::separate(invalid, battle_menu::MovesInfo, battle_menu::MovesInfo, 14);
    require(invalid.panels[2].origin_y == p.panels[2].origin_y &&
                invalid.panels[1].origin_y == p.panels[1].origin_y,
            "out-of-range or identical indices change nothing");
    auto message = p;
    menu_text::separate(message, 0, battle_menu::MovesList, 14);
    require(message.panels[0].origin_y == p.panels[0].origin_y, "the empty message never moves");
    for (int row = 13; row <= 16; ++row)
        tiles[row * 20 + 6] = 0x7f;
    tiles[10 * 20 + 2] = 0x7f;
    tiles[11 * 20 + 7] = 0x7f;
    tiles[13 * 20 + 5] = 0x6e;
    p = plan(tiles, layout);
    require(p.panels[1].fallback && !p.panels[2].fallback,
            "unsupported move glyph only falls back in its own window");
    tiles[17 * 20 + 19] = 0;
    require(battle_menu::classify(tiles.data()).kind == battle_menu::Kind::Unknown,
            "broken battle border retains original composition");
    require((argc - 1) % 2 == 0, "fixture arguments: TILEMAP EXPECTED_KIND");
    for (int i = 1; i < argc; i += 2) {
        FILE *f = std::fopen(argv[i], "rb");
        require(f, "private battle tilemap opens");
        require(std::fread(tiles.data(), 1, 360, f) == 360 && std::fgetc(f) == EOF,
                "private battle tilemap size");
        std::fclose(f);
        layout = battle_menu::classify(tiles.data());
        require(int(layout.kind) == std::atoi(argv[i + 1]),
                "original battle window classification");
        std::printf("%s: kind=%d regions=%zu\n", argv[i], int(layout.kind), layout.regions.count);
    }
    std::puts("PASS: message/fight/moves, PP overlap, empty suppression and original fallback");
}
