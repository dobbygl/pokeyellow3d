#pragma once
#include "item_menu_state.h"
#include "mon_pic.h"
#include <cstring>

namespace battle_bag {
// ScaleSpriteByTwo in engine/battle/scale_sprites.asm crops the unused bottom
// and right four pixels of a 4x4-tile back image, then doubles each remaining
// pixel. The compressed pointer is wMonHBackSprite, BaseStats+13.
inline mon_pic::Picture back_picture(const uint8_t *rom, size_t size, int species) {
    int number = mon_pic::number(rom, size, species);
    if (!number)
        return {};
    size_t header = 0x383de + (number - 1) * 28;
    int pointer = rom[header + 13] | (rom[header + 14] << 8);
    if (rom[header] != number || pointer < 0x4000 || pointer >= 0x8000)
        return {};
    int bank = species < 0x1f   ? 9
               : species < 0x4a ? 10
               : species < 0x74 ? 11
               : species < 0x99 ? 12
                                : 13;
    auto raw =
        mon_pic::decompress(rom + bank * 0x4000 + (pointer & 0x3fff), 0x4000 - (pointer & 0x3fff));
    if (!raw.valid || raw.width != 4 || raw.height != 4)
        return {};
    mon_pic::Picture out;
    out.width = out.height = 7;
    out.valid = true;
    for (int y = 0; y < 56; ++y)
        for (int x = 0; x < 56; ++x)
            for (int plane = 0; plane < 2; ++plane) {
                // decompress() centered the 4x4 source at (16,24).
                int sx = x / 2 + 16, sy = y / 2 + 24;
                int bit = (raw.tiles[(sx / 8 * 56 + sy) * 2 + plane] >> (7 - sx % 8)) & 1;
                out.tiles[(x / 8 * 56 + y) * 2 + plane] |= uint8_t(bit << (7 - x % 8));
            }
    return out;
}
struct Cache {
    const uint8_t *rom = nullptr;
    int enemy = 0, player = 0;
    mon_pic::Picture front, back;
    void update(const GBContext *ctx) {
        if (rom != ctx->rom) {
            *this = {};
            rom = ctx->rom;
        }
        // Original image species, before Transform changes the battle stats.
        int next_enemy = battle::read(ctx, 0xcfd7), next_player = battle::read(ctx, 0xcfd8);
        if (enemy != next_enemy) {
            enemy = next_enemy;
            front = mon_pic::front(ctx->rom, ctx->rom_size, enemy);
        }
        if (player != next_player) {
            player = next_player;
            back = back_picture(ctx->rom, ctx->rom_size, player);
        }
    }
};
struct Graphic {
    size_t address = 0;
    bool one_bit = false;
};
inline Graphic graphic(int tile) {
    // LoadHudTilePatterns overwrites LV (6E); it is not the PC's LV asset.
    if (tile == 0x6e)
        return {0x10c08, true};
    if (tile == 0x73 || tile == 0x74 || tile == 0x76)
        return {size_t(0x10c18 + (tile - 0x73) * 8), true};
    if (tile == 0x62 || tile == 0x71)
        return {size_t(0x10a20 + (tile - 0x62) * 16), false};
    return {};
}
inline int portrait_cell(int x, int y) {
    if (x >= 12 && x <= 18 && y <= 1)
        return (x - 12) * 7 + y;
    if (x >= 1 && x <= 3 && y >= 5 && y <= 11)
        return 0x31 + (x - 1) * 7 + y - 5;
    return -1;
}
struct Snapshot {
    bool ready = false;
    std::array<uint8_t, 360> text{};
    std::array<bool, 360> graphics{};
};
inline Snapshot prepare(const GBContext *ctx, Cache &cache) {
    Snapshot out;
    if (item_menu::context(ctx) != full_menu::Context::BattleBag ||
        full_menu::classify(ctx->wram + 0x3a0, full_menu::Context::BattleBag).kind ==
            full_menu::Kind::Unknown ||
        ctx->wram[0xc26] > ctx->wram[0xc28])
        return out;
    cache.update(ctx);
    if (!cache.front.valid || !cache.back.valid ||
        std::memcmp(cache.front.tiles.data(), ctx->vram + 0x1000, mon_pic::TileBytes) ||
        std::memcmp(cache.back.tiles.data(), ctx->vram + 0x1310, mon_pic::TileBytes))
        return out;
    std::copy_n(ctx->wram + 0x3a0, 360, out.text.begin());
    for (int y = 0; y < 18; ++y)
        for (int x = 0; x < 20; ++x) {
            size_t cell = size_t(y * 20 + x);
            int tile = out.text[cell];
            bool window = y >= 12 || (x >= 4 && y >= 2 && y <= 12);
            if (window) {
                if (tile < 0x79)
                    return out;
                continue;
            }
            int pic = portrait_cell(x, y);
            if (pic >= 0) {
                if (tile != pic)
                    return out;
            } else {
                if (tile == 0x7f || (tile >= 0x80 && y <= 1 && x >= 1 && x <= 10))
                    continue;
                bool hud =
                    (x == 4 && y == 1 && tile == 0x6e) ||
                    (y == 2 && ((x == 1 && tile == 0x73) || (x == 2 && tile == 0x71) ||
                                (x == 3 && tile == 0x62))) ||
                    (y == 3 && ((x == 1 && tile == 0x74) || ((x == 2 || x == 3) && tile == 0x76)));
                auto source = graphic(tile);
                if (!hud || !source.address)
                    return out;
                for (int row = 0; row < 8; ++row)
                    for (int plane = 0; plane < 2; ++plane)
                        if (ctx->vram[0x1000 + tile * 16 + row * 2 + plane] !=
                            ctx->rom[source.address + row * (source.one_bit ? 1 : 2) +
                                     (source.one_bit ? 0 : plane)])
                            return out;
            }
            out.graphics[cell] = true;
            out.text[cell] = 0x7f;
        }
    out.ready = true;
    return out;
}
} // namespace battle_bag
