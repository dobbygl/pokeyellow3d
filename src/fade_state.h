#pragma once
#include "gbrt.h"
#include <cstddef>
#include <cstdint>

namespace fade {
// BGP maps four source shades to LCD shades (0 = white, 3 = black).
// Give each source shade weight 1/4: E4 has mean luminance 1/2.
// Preserve the scene at E4; interpolate toward black/white relative to that
// baseline. No presentation timer changes the duration of a guest palette.
struct Tone {
    float multiply = 1, add = 0;
};
constexpr Tone tone(uint8_t bgp) {
    int sum = 0;
    for (int i = 0; i < 4; i++)
        sum += 3 - ((bgp >> (i * 2)) & 3);
    return sum <= 6 ? Tone{sum / 6.f, 0} : Tone{(12 - sum) / 6.f, (sum - 6) / 6.f};
}
constexpr bool palette(uint8_t bgp) {
    return bgp == 0xe4 || bgp == 0xf9 || bgp == 0xfe || bgp == 0xff || bgp == 0x90 || bgp == 0x40 ||
           bgp == 0;
}
// Canonical Yellow home-bank CALL sites, verified against ROM and symbols.
// A live aligned stack return is necessary: persistent warp variables alone
// also survive resets, menus and battles and cannot identify this lifetime.
inline bool home_call(const GBContext *ctx, size_t call, uint16_t target, uint8_t opcode = 0xcd) {
    if (!ctx || !ctx->rom || !ctx->wram || call + 2 >= ctx->rom_size || call >= 0x4000 ||
        ctx->rom[call] != opcode || ctx->rom[call + 1] != (target & 255) ||
        ctx->rom[call + 2] != (target >> 8) || ctx->sp < 0xd000 || ctx->sp >= 0xe000)
        return false;
    const auto ret = call + 3;
    for (int a = ctx->sp; a < 0xdfff; a += 2)
        if (ctx->wram[a - 0xc000] == (ret & 255) && ctx->wram[a - 0xbfff] == (ret >> 8))
            return true;
    return false;
}
inline bool warp(const GBContext *ctx) {
    return home_call(ctx, 0x576, 0x1eb6) || // Rock Tunnel's special fade
           home_call(ctx, 0x581, 0x6ef) || home_call(ctx, 0x5a7, 0x6ef) ||
           home_call(ctx, 0x5c9, 0x6ef) || // PlayMapChangeSound -> GBFadeOutToBlack
           home_call(ctx, 0x5d5, 0xfc3) || // IgnoreInputForHalfSecond
           home_call(ctx, 0x1dc, 0xecb) || // EnterMap -> LoadMapData
           // MapEntryAfterBattle runs from EnterMap, outside text dispatch. The
           // value 0200 also occurs as a saved register pair in StatusScreen2;
           // a matching stack word alone would hide its entire second page.
           (home_call(ctx, 0x1fd, 0x750, 0xc4) && !home_call(ctx, 0x2de, 0x2817) &&
            !home_call(ctx, 0x3f47, 0x2817));
}
} // namespace fade
