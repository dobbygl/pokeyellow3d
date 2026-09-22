#pragma once
#include "full_menu_layout.h"
#include "menu_text_gl.h"
#include "pc_state.h"
#include "item_menu_state.h"
#include "pokemon_menu_gl.h"

namespace full_menu {
inline PalletFullMenuInfo shown{};
inline Context context(pc_state::Mode mode) {
    switch (mode) {
    case pc_state::Mode::Center:
        return Context::PcCenter;
    case pc_state::Mode::Items:
        return Context::PcItems;
    case pc_state::Mode::Bill:
        return Context::PcBill;
    case pc_state::Mode::Oak:
        return Context::PcOak;
    default:
        return Context::None;
    }
}
inline void draw_context(GBContext *ctx, Context domain, int width, int height,
                         bool verified_background = true) {
    auto layout = classify(ctx->wram + 0x3a0, domain);
    shown = {true, true, int(domain), int(layout.kind), int(layout.text.count)};
    auto selection = list_snapshot(ctx->wram);
    shown.current = selection.current;
    shown.scroll = selection.scroll;
    shown.selected = selection.selected;
    shown.cursor_x = selection.cursor_x;
    shown.cursor_y = selection.cursor_y;
    menu_text::prime();
    if (!verified_background || layout.kind == Kind::Unknown ||
        selection.current > ctx->wram[0xc28] ||
        ((domain == Context::Bag || domain == Context::Mart) &&
         !item_menu::text_only(ctx->wram + 0x3a0, layout.text))) {
        lcd_overlay::framed(gb_get_framebuffer(ctx), float(width), float(height));
        return;
    }
    auto drawn = menu_text::regions(ctx->wram + 0x3a0, ctx->vram, ctx->rom, ctx->rom_size,
                                    gb_get_framebuffer(ctx), layout.text, float(width),
                                    float(height), menu_text::Placement::Full);
    shown.fallback = drawn.panels == 0;
}
inline void draw(GBContext *ctx, pc_state::Mode mode, int width, int height,
                 bool verified_background = true) {
    if (verified_background && pokemon_menu::draw(ctx, width, height))
        return;
    draw_context(ctx, context(mode), width, height, verified_background);
}
inline bool draw_items(GBContext *ctx, int width, int height) {
    auto domain = item_menu::context(ctx);
    if (domain == Context::None)
        return false;
    draw_context(ctx, domain, width, height);
    return true;
}
} // namespace full_menu
