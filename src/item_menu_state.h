#pragma once
#include "battle_state.h"
#include "full_menu_layout.h"

namespace item_menu {
inline bool live_call(const GBContext *ctx, size_t offset, int target) {
    if (ctx->sp < 0xd000 || ctx->sp >= 0xe000 || offset + 3 > ctx->rom_size ||
        ctx->rom[offset] != 0xcd || ctx->rom[offset + 1] != (target & 255) ||
        ctx->rom[offset + 2] != (target >> 8))
        return false;
    int returned = int(offset < 0x4000 ? offset : 0x4000 + offset % 0x4000) + 3;
    for (int at = ctx->sp; at < 0xdfff; at += 2)
        if ((battle::read(ctx, at) | (battle::read(ctx, at + 1) << 8)) == returned)
            return true;
    return false;
}
inline full_menu::Context context(const GBContext *ctx) {
    if (!ctx || !ctx->wram || !ctx->vram || !ctx->io || !ctx->rom || ctx->rom_size != 1048576 ||
        (ctx->io[0x40] & 0x90) != 0x80 || ctx->io[0x47] != 0xe4)
        return full_menu::Context::None;
    if (battle::read(ctx, battle::IsInBattle))
        return battle::normal(ctx) && live_call(ctx, 0x3d14e, 0x2ae0)
                   ? full_menu::Context::BattleBag
                   : full_menu::Context::None;
    // DisplayPokemartDialogue -> DisplayPokemartDialogue_ (fixed-bank caller).
    if (live_call(ctx, 0x292e, 0x69a5))
        return full_menu::Context::Mart;
    // StartMenu_Item: list, Use/Toss, quantity, UseItem and TossItem. CALL bytes
    // and live stacks verified against the pinned pret source and private states.
    if (live_call(ctx, 0x11ed2, 0x2ae0) || live_call(ctx, 0x11f21, 0x3aab) ||
        live_call(ctx, 0x11fc5, 0x2c51) || live_call(ctx, 0x11f7a, 0x2fe4) ||
        live_call(ctx, 0x11fce, 0x2fec))
        return full_menu::Context::Bag;
    return full_menu::Context::None;
}
inline bool text_only(const uint8_t *tiles, const menu_layout::Layout &layout) {
    // PC graphics sharing these tile IDs must not leak into an item profile.
    for (size_t i = 0; i < layout.count; ++i) {
        auto r = layout.regions[i];
        for (int y = r.y; y < r.y + r.h; ++y)
            for (int x = r.x; x < r.x + r.w; ++x)
                if (tiles[y * 20 + x] < 0x79)
                    return false;
    }
    return true;
}
} // namespace item_menu
