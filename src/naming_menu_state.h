#pragma once
#include "item_menu_state.h"
#include "pokemon_menu_state.h"

namespace naming_menu {
// Original names: wNamingScreenType, wNamingScreenNameLength, wAlphabetCase.
// Generated aliases: wTempTilesetNumTiles, wTownMapCoords, wHPBarOldHP.
constexpr int Type = 0xd07c, Length = 0xcee9, Case = 0xceeb;
inline bool active(const GBContext *ctx) {
    if (!pokemon_menu::ready(ctx))
        return false;
    // AskName, DisplayNameRaterScreen, ChoosePlayerName, ChooseRivalName.
    for (size_t call : {0x62a3, 0x62d9, 0x66ff, 0x6747})
        if (item_menu::live_call(ctx, call, 0x6307))
            return true;
    return false;
}
inline bool cursor_cell(int x, int y) {
    return (y == 15 && x == 1) || (y >= 5 && y <= 13 && y % 2 && x >= 1 && x <= 17 && x % 2);
}
struct Snapshot {
    bool ready = false;
    int type = -1, cursor_x = -1, cursor_y = -1;
    std::array<uint8_t, 360> text{};
    std::array<bool, 360> graphics{};
};
inline Snapshot prepare(const GBContext *ctx) {
    Snapshot out;
    if (!active(ctx) || full_menu::classify(ctx->wram + 0x3a0, full_menu::Context::Naming).kind !=
                            full_menu::Kind::Naming)
        return out;
    out.type = battle::read(ctx, Type);
    int length = battle::read(ctx, Length), letter_case = battle::read(ctx, Case);
    int row = battle::read(ctx, 0xcc26), column = battle::read(ctx, 0xcc25);
    if (out.type > 2 || length > (out.type == 2 ? 10 : 7) || letter_case > 1 || row > 6 ||
        column < 1 || column > 17 || !(column % 2) ||
        !pokemon_menu::icons_valid(ctx, out.type == 2 ? 1 : 0))
        return out;
    // AnimatePartyMon_ForceSpeed1 temporarily uses current item zero, including
    // on player/rival screens. A live arrow is the authoritative painted row.
    std::copy_n(ctx->wram + 0x3a0, 360, out.text.begin());
    int arrows = 0;
    size_t alphabet = letter_case ? 25884 : 25940;
    for (int row = 0; row < 5; ++row)
        for (int col = 0; col < 9; ++col)
            if (out.text[(5 + row * 2) * 20 + 2 + col * 2] != ctx->rom[alphabet + row * 9 + col])
                return out;
    for (int i = 0; i < 10; ++i)
        if (out.text[15 * 20 + 2 + i] != ctx->rom[alphabet + 45 + i])
            return out;
    for (int y = 0; y < 18; ++y)
        for (int x = 0; x < 20; ++x) {
            size_t cell = size_t(y * 20 + x);
            int tile = out.text[cell];
            if (tile == 0xed) {
                if (!cursor_cell(x, y))
                    return out;
                out.cursor_x = x;
                out.cursor_y = y;
                ++arrows;
            }
            bool underscore = y == 3 && x >= 10 && x < (out.type == 2 ? 20 : 17);
            bool end = x == 18 && y == 13;
            if (underscore || end) {
                if ((underscore && tile != 0x76 && tile != 0x77) || (end && tile != 0xf0))
                    return out;
                size_t source = end ? 0x64e5 : 0x10b60 + size_t(tile - 0x76) * 16;
                size_t dest = end ? 0xf00 : 0x1000 + size_t(tile) * 16;
                for (int row = 0; row < 8; ++row)
                    for (int plane = 0; plane < 2; ++plane)
                        if (ctx->vram[dest + row * 2 + plane] !=
                            ctx->rom[source + row * (end ? 1 : 2) + (end ? 0 : plane)])
                            return out;
                out.graphics[cell] = true;
                out.text[cell] = 0x7f;
            } else if (tile < 0x79 || tile == 0xf0)
                return out;
        }
    out.ready = arrows == 1;
    return out;
}
} // namespace naming_menu
