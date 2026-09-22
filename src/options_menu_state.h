#pragma once
#include "item_menu_state.h"

namespace options_menu {
// wOptionsCursorLocation aliases wWhichTrade in pokeyellow_internal.h.
// InitOptionsMenu and OptionsControl use five rows followed by Cancel at 7.
constexpr int Cursor = 0xcd3d;
inline bool row(int value) {
    return (value >= 0 && value <= 4) || value == 7;
}
inline bool active(const GBContext *ctx) {
    // DisplayOptionMenu's far call is shared by Start and the title menu.
    return ctx && ctx->rom && ctx->rom_size == 1048576 && ctx->wram && ctx->vram && ctx->io &&
           !battle::read(ctx, battle::IsInBattle) && (ctx->io[0x40] & 0x90) == 0x80 &&
           ctx->io[0x47] == 0xe4 && item_menu::live_call(ctx, 0x5df7, 0x3e84);
}
struct Selection {
    bool valid = false;
    int current = 0, painted = -1;
};
inline Selection selection(const GBContext *ctx) {
    Selection result;
    if (!ctx || !ctx->wram)
        return result;
    result.current = battle::read(ctx, Cursor);
    if (!row(result.current))
        return result;
    int arrows = 0;
    for (int cell = 0; cell < 360; ++cell) {
        int tile = ctx->wram[0x3a0 + cell];
        if (tile < 0x79)
            return result;
        if (tile != 0xed)
            continue;
        int x = cell % 20, y = cell / 20;
        if (x != 1 || y < 2 || y % 2 || !row((y - 2) / 2))
            return result;
        result.painted = (y - 2) / 2;
        ++arrows;
    }
    // OptionsControl can update the logical row before repainting the arrow.
    // Keep the single original painted arrow, including that intermediate frame.
    result.valid = arrows == 1;
    return result;
}
} // namespace options_menu
