#pragma once
#include "gbrt.h"
#include <array>
#include <cstring>

namespace title_state {
enum class Phase { None, Title, Menu, Continue, NewGame };
struct Call {
    uint16_t address, target;
};
// Init tail-jumps to PrepareTitleScreen, which tail-jumps to MainMenu.
// Consequently its yielding CALL is the outermost live stack word at DFFD,
// not an arbitrary value anywhere in RAM (which can be a saved register).
inline bool root_call(const GBContext *ctx, Call call) {
    return ctx && ctx->rom && ctx->rom_size > size_t(call.address) + 2 && ctx->wram &&
           ctx->sp >= 0xd000 && ctx->sp <= 0xdffd && ((0xdffd - ctx->sp) % 2 == 0) &&
           ctx->rom[call.address] == 0xcd && ctx->rom[call.address + 1] == (call.target & 255) &&
           ctx->rom[call.address + 2] == (call.target >> 8) &&
           (ctx->wram[0x1ffd] | (ctx->wram[0x1ffe] << 8)) == call.address + 3;
}
inline bool logo(const GBContext *ctx) {
    // Yellow's symbol vTitleLogo ($8800) holds Pikachu, despite its name.
    // The Pokemon lettering is at $9000. Both transfers come from bank $3d.
    return ctx && ctx->rom && ctx->rom_size >= 0xf525b && ctx->vram &&
           !std::memcmp(ctx->vram + 0x1000, ctx->rom + 0xf46fb, 0x730) &&
           !std::memcmp(ctx->vram + 0x800, ctx->rom + 0xf4e5b, 0x400);
}
inline Phase sample(const GBContext *ctx) {
    if (!ctx || !ctx->wram || !ctx->hram || !ctx->io || !ctx->rom || !ctx->vram)
        return Phase::None;
    constexpr Call title_calls[]{
        {0x41d0, 0x3e84}, {0x41d3, 0x4418}, {0x41d6, 0x4241}, {0x41d9, 0x36ec}, {0x41dc, 0x36f8},
        {0x41df, 0x007b}, {0x41e7, 0x3e84}, {0x41ec, 0x4332}, {0x41ef, 0x370f}, {0x41f6, 0x36f8},
        {0x41fb, 0x4332}, {0x4200, 0x3e05}, {0x4203, 0x3de0}, {0x420a, 0x3040}, {0x4223, 0x4237},
        {0x4260, 0x371b}, {0x4265, 0x372f}, {0x426a, 0x2238}, {0x4272, 0x3e84}, {0x4279, 0x3ddb},
        {0x427e, 0x4387}, {0x4281, 0x373e}, {0x4284, 0x2233}, {0x428c, 0x2238}, {0x42a4, 0x4405},
        {0x42aa, 0x1e64}, {0x42ad, 0x381e}, {0x42c1, 0x4387}, {0x42c4, 0x3dd8}, {0x42c7, 0x0082},
        {0x42d0, 0x16dd}, {0x42d5, 0x4332}, {0x42da, 0x4332}, {0x42dd, 0x3ddb}, {0x42e0, 0x1e6f},
        {0x431e, 0x2233}};
    if (logo(ctx)) {
        for (auto call : title_calls)
            if (root_call(ctx, call))
                return Phase::Title;
        // The idle timeout spends ~104 frames in a busy audio-fade loop,
        // without a CALL on the stack. Verify that exact executing loop too;
        // otherwise the title would flash back to 2D before Init clears VRAM.
        constexpr uint8_t audio_loop[]{0xfa, 0xc6, 0xcf, 0xa7, 0x20, 0xfa, 0xc3, 0x10, 0x1d};
        if (ctx->sp == 0xdfff && ctx->hram[0x38] == 1 && ctx->wram[0xfc6] &&
            (ctx->pc == 0x4321 || ctx->pc == 0x4324 || ctx->pc == 0x4325) &&
            !std::memcmp(ctx->rom + 0x4321, audio_loop, sizeof(audio_loop)))
            return Phase::Title;
    }
    const int save = ctx->wram[0x1087]; // wSaveFileStatus: 1 absent, 2 valid.
    if (save != 1 && save != 2)
        return Phase::None;
    if (root_call(ctx, {0x5cd7, 0x5e85}) || root_call(ctx, {0x5ce1, 0x372f}))
        return Phase::NewGame;
    if (save == 2 && root_call(ctx, {0x5c64, 0x5d1f}))
        return Phase::Continue;
    constexpr Call menu_calls[]{
        {0x5bb1, 0x5dfb}, {0x5bb8, 0x3eb4}, {0x5bbd, 0x372f}, {0x5bd3, 0x16dd}, {0x5bd6, 0x3e03},
        {0x5bd9, 0x36a3}, {0x5bdc, 0x3683}, {0x5bf1, 0x16f0}, {0x5bfa, 0x1723}, {0x5c05, 0x16f0},
        {0x5c0e, 0x1723}, {0x5c16, 0x231c}, {0x5c36, 0x3aab}, {0x5c40, 0x372f}, {0x5c59, 0x5df2}};
    for (auto call : menu_calls)
        if (root_call(ctx, call))
            return Phase::Menu;
    return Phase::None;
}
} // namespace title_state
