#pragma once
#include "item_menu_state.h"
#include "mon_pic.h"
#include <algorithm>
#include <cstring>

namespace trainer_card {
inline bool active(const GBContext *ctx) {
    return ctx && ctx->rom && ctx->rom_size == 1048576 && ctx->wram && ctx->vram && ctx->io &&
           !battle::read(ctx, battle::IsInBattle) && (ctx->io[0x40] & 0x90) == 0x80 &&
           ctx->io[0x47] == 0xe4 && item_menu::live_call(ctx, 0x12029, 0x3852);
}
struct Cache {
    const uint8_t *rom = nullptr;
    mon_pic::Picture red;
    void update(const GBContext *ctx) {
        if (rom == ctx->rom)
            return;
        rom = ctx->rom;
        // DrawTrainerInfo: bank 04, RedPicFront at 5A97. CopyData then moves
        // tiles 7..34 onto 0..27, cropping the first column of the 7x7 portrait.
        red = mon_pic::decompress(rom + 0x11a97, 0x6569);
    }
};
inline bool border_cell(int x, int y) {
    for (auto r : {menu_layout::Rect{0, 0, 20, 8}, menu_layout::Rect{1, 10, 18, 8}})
        if (menu_layout::contains(r, x, y) && menu_layout::border(r, x, y))
            return true;
    return false;
}
// The expected tile position follows DrawTrainerInfo/DrawBadges, independently
// of the character encoding. D6..DF replace font slots and are never text.
inline int graphic_cell(int x, int y, int badges) {
    if (x >= 15 && x <= 18 && y >= 1 && y <= 6)
        return (x - 15) * 7 + y - 1;
    if ((x == 0 || x == 19) && y >= 10)
        return 0xd7;
    if ((x == 6 || x == 13) && y == 9)
        return 0x76;
    for (int badge = 0; badge < 8; ++badge) {
        int left = 2 + (badge % 4) * 4, top = 11 + (badge / 4) * 3;
        if (x == left && y == top)
            return 0xd8 + badge;
        if (x >= left + 1 && x <= left + 2) {
            if (y == top && !(badges & (1 << badge)))
                return 0x60 + badge * 2 + x - left - 1;
            if (y == top + 1 || y == top + 2)
                return 0x20 + badge * 8 + ((badges & (1 << badge)) ? 4 : 0) + (y - top - 1) * 2 +
                       x - left - 1;
        }
    }
    return -1;
}
inline size_t graphic_offset(int tile) {
    if (tile >= 0x20 && tile <= 0x5f)
        return 59675 + size_t(tile - 0x20) * 16;
    if (tile >= 0x60 && tile <= 0x76)
        return 1006772 + size_t(tile - 0x60) * 16;
    if (tile == 0xd6)
        return 0x10ee8; // TextBoxGraphics tile 13, original colon.
    if (tile == 0xd7)
        return 1006756; // TrainerInfoTextBoxTileGraphics tile 8, side background.
    if (tile >= 0xd8 && tile <= 0xdf)
        return 1007140 + size_t(tile - 0xd8) * 16;
    return 0;
}
struct Snapshot {
    bool ready = false;
    std::array<uint8_t, 360> text{};
    std::array<bool, 360> graphics{};
};
inline Snapshot prepare(const GBContext *ctx, Cache &cache) {
    Snapshot out;
    if (!active(ctx) ||
        full_menu::classify(ctx->wram + 0x3a0, full_menu::Context::TrainerCard).kind !=
            full_menu::Kind::TrainerCard)
        return out;
    cache.update(ctx);
    if (!cache.red.valid || std::memcmp(ctx->vram + 0x1000, cache.red.tiles.data() + 112, 448) ||
        std::memcmp(ctx->vram + 0x1770, ctx->rom + 1006628, 128))
        return out;
    std::copy_n(ctx->wram + 0x3a0, 360, out.text.begin());
    int badges = battle::read(ctx, 0xd355);
    for (int y = 0; y < 18; ++y)
        for (int x = 0; x < 20; ++x) {
            size_t cell = size_t(y * 20 + x);
            int tile = out.text[cell];
            if (border_cell(x, y)) {
                // Geometry was checked above; normalize only private decoration.
                if (tile == 0x77 || tile == 0x78)
                    out.text[cell] = 0x7a;
                continue;
            }
            int expected = graphic_cell(x, y, badges);
            if (y == 6 && x >= 10 && x <= 12 && tile == 0xd6)
                expected = 0xd6;
            if (expected >= 0) {
                if (tile != expected)
                    return out;
                size_t source = graphic_offset(tile);
                size_t vram = tile < 0x80 ? 0x1000 + tile * 16 : size_t(tile) * 16;
                if (source && std::memcmp(ctx->vram + vram, ctx->rom + source, 16))
                    return out;
                out.graphics[cell] = true;
                out.text[cell] = 0x7f;
            } else {
                bool text = ((y == 2 || y == 4 || y == 6) && x >= 2 && x <= 14) ||
                            (y == 9 && x >= 7 && x <= 12);
                if (tile != 0x7f &&
                    (!text || tile < 0x80 || tile == 0xed || (tile >= 0xd6 && tile <= 0xdf)))
                    return out;
            }
        }
    out.ready = true;
    return out;
}
} // namespace trainer_card
