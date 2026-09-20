#pragma once
#include "dex_state.h"
namespace dex_area_state {
inline bool active(const GBContext *ctx) {
    return ctx && ctx->rom && ctx->rom_size == 1048576 && ctx->wram && ctx->io && ctx->vram &&
           !battle::read(ctx, battle::IsInBattle) && ctx->rom[0x40116] == 0x3e &&
           ctx->rom[0x40117] == 0x4a && battle::live_return(ctx, 0x40061, 0x4070) &&
           battle::live_return(ctx, 0x40118, 0x3eb4) &&
           mon_pic::number(ctx->rom, ctx->rom_size, battle::read(ctx, dex_state::Current));
}
inline bool ready(const GBContext *ctx) {
    return active(ctx) && battle::live_return(ctx, 0x71003, 0x3852) && (ctx->io[0x40] & 0x80) &&
           ctx->io[0x47] == 0xe4 && battle::read(ctx, 0xd09a);
}
inline bool title_ready(const GBContext *ctx, const uint32_t *framebuffer) {
    if (!ready(ctx) || !framebuffer)
        return false;
    uint32_t background = framebuffer[0];
    for (int tile = 0; tile < 20; tile++) {
        int id = battle::tile(ctx, tile, 0);
        int at = (ctx->io[0x40] & 16) ? id * 16 : 0x1000 + int(int8_t(id)) * 16;
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) {
                bool ink = ((ctx->vram[at + y * 2] | ctx->vram[at + y * 2 + 1]) >> (7 - x)) & 1;
                if ((framebuffer[y * 160 + tile * 8 + x] != background) != ink)
                    return false;
            }
    }
    return true;
}
inline bool blink(const GBContext *ctx) {
    return ready(ctx) && battle::read(ctx, 0xd08a) < 25;
}
} // namespace dex_area_state
