#pragma once
#include "battle_state.h"
#include "mon_pic.h"

namespace dex_state {
constexpr uint16_t Current = 0xd11d, Owned = 0xd2f6, Seen = 0xd309;
constexpr battle::Rect Portrait{1, 1, 0};
enum class Registration { Absent, Seen, Caught };
inline bool flag(const GBContext *ctx, int address, int n) {
    return n >= 1 && n <= 151 && (battle::read(ctx, address + (n - 1) / 8) & (1 << ((n - 1) % 8)));
}
inline int count(const GBContext *ctx, int address) {
    int total = 0;
    for (int i = 0; i < 19; i++) {
        int byte = battle::read(ctx, address + i);
        for (int bit = 0; bit < 8; bit++)
            total += (byte >> bit) & 1;
    }
    return total;
}
struct Selection {
    int number = 0, species = 0, seen = 0, caught = 0;
    Registration registration = Registration::Absent;
    bool side = false;
    int row = 0;
};
inline bool visible(const GBContext *ctx, const uint32_t *framebuffer, const Selection &selection) {
    if (!framebuffer || !selection.number || selection.row < 0 || selection.row > 6)
        return false;
    int digits[] = {selection.number / 100, (selection.number / 10) % 10, selection.number % 10};
    uint32_t background = framebuffer[0];
    for (int digit = 0; digit < 3; digit++)
        for (int y = 0; y < 8; y++) {
            int tile = 0xf6 + digits[digit],
                at = (ctx->io[0x40] & 16) ? tile * 16 : 0x1000 + int(int8_t(tile)) * 16;
            int ink = ctx->vram[at + y * 2] | ctx->vram[at + y * 2 + 1];
            for (int x = 0; x < 8; x++) {
                bool pixel =
                    framebuffer[((2 + selection.row * 2) * 8 + y) * 160 + (1 + digit) * 8 + x] !=
                    background;
                if (pixel != bool(ink & (1 << (7 - x))))
                    return false;
            }
        }
    // The list's filled/unfilled cursor must already be on that displayed row.
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            if (framebuffer[((3 + selection.row * 2) * 8 + y) * 160 + x] != background)
                return true;
    return false;
}
inline Selection list(const GBContext *ctx) {
    if (!ctx || !ctx->wram || !ctx->rom || !ctx->vram || !ctx->io || ctx->rom_size != 1048576 ||
        battle::read(ctx, battle::IsInBattle) || !(ctx->io[0x40] & 0x80) || ctx->io[0x47] != 0xe4)
        return {};
    bool list = battle::live_return(ctx, 0x4003d, 0x4140),
         side = battle::live_return(ctx, 0x40061, 0x4070);
    if ((!list && !side) || battle::live_return(ctx, 0x40104, 0x4323) ||
        battle::tile(ctx, 14, 0) != 0x71 || battle::tile(ctx, 1, 1) != 0x82 ||
        battle::tile(ctx, 16, 1) != 0x92 || battle::tile(ctx, 16, 4) != 0x8e)
        return {};
    int n = list ? battle::read(ctx, 0xcc26) + battle::read(ctx, 0xcc36) + 1
                 : mon_pic::number(ctx->rom, ctx->rom_size, battle::read(ctx, Current));
    int species = mon_pic::species(ctx->rom, ctx->rom_size, n);
    if (!species)
        return {};
    auto registration = flag(ctx, Owned, n)  ? Registration::Caught
                        : flag(ctx, Seen, n) ? Registration::Seen
                                             : Registration::Absent;
    int row = side ? n - battle::read(ctx, 0xcc36) - 1 : battle::read(ctx, 0xcc26);
    return {n, species, count(ctx, Seen), count(ctx, Owned), registration, side, row};
}
inline bool data(const GBContext *ctx) {
    if (!ctx || !ctx->wram || !ctx->rom || !ctx->vram || !ctx->io || ctx->rom_size != 1048576 ||
        battle::read(ctx, battle::IsInBattle) || !(ctx->io[0x40] & 0x80) || ctx->io[0x47] != 0xe4)
        return false;
    // HandlePokedexSideMenu.choseData -> ShowPokedexDataInternal, bank 10.
    // A live return alone is insufficient while VRAM/the tilemap are changing.
    if (!battle::live_return(ctx, 0x40104, 0x4323) ||
        !mon_pic::number(ctx->rom, ctx->rom_size, battle::read(ctx, Current)) ||
        battle::tile(ctx, 0, 0) != 0x63 || battle::tile(ctx, 19, 17) != 0x6e)
        return false;
    bool window = (ctx->io[0x40] & 0x20) && ctx->io[0x4a] == 0 && ctx->io[0x4b] == 7;
    int map = (window ? (ctx->io[0x40] & 0x40) : (ctx->io[0x40] & 8)) ? 0x1c00 : 0x1800;
    if (ctx->io[0x40] & 16)
        return false; // vFrontPic uses signed tile IDs at 9000.
    for (int y = 0; y < 7; y++)
        for (int x = 0; x < 7; x++) {
            int id = (6 - x) * 7 + y;
            if (battle::tile(ctx, x + 1, y + 1) != id ||
                ctx->vram[map + (y + 1) * 32 + x + 1] != id)
                return false;
        }
    return true;
}
} // namespace dex_state
