#pragma once
#include "full_menu_layout.h"
#include "menu_text_gl.h"
#include "pc_state.h"
#include "item_menu_state.h"
#include "pokemon_menu_gl.h"
#include "battle_bag_state.h"
#include "options_menu_state.h"
#include "trainer_card_state.h"
#include "naming_menu_gl.h"

namespace full_menu {
inline PalletFullMenuInfo shown{};
inline battle_bag::Cache battle_pictures;
inline pokemon_menu::Texture<160, 144> battle_graphics;
inline trainer_card::Cache trainer_picture;
inline pokemon_menu::Texture<160, 144> trainer_graphics;
inline void shutdown() {
    battle_graphics.reset();
    battle_pictures = {};
    trainer_graphics.reset();
    trainer_picture = {};
    naming_menu::graphics.reset();
    shown = {};
}
inline bool upload_battle_graphics(const GBContext *ctx, const battle_bag::Snapshot &snapshot) {
    std::array<uint8_t, 160 * 144 * 4> rgba{};
    constexpr ImU32 colors[]{0, ui_theme::DimInk, ui_theme::FrameEdge | IM_COL32_A_MASK,
                             ui_theme::Ink};
    for (int cell = 0; cell < 360; ++cell) {
        if (!snapshot.graphics[size_t(cell)])
            continue;
        int tx = cell % 20, ty = cell / 20, tile = ctx->wram[0x3a0 + cell];
        bool portrait = battle_bag::portrait_cell(tx, ty) >= 0;
        auto palette =
            battle::palette(ctx, tx >= 12 ? battle_pictures.enemy : battle_pictures.player);
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                size_t at = 0x1000 + tile * 16 + y * 2;
                int value =
                    ((ctx->vram[at] >> (7 - x)) & 1) | (((ctx->vram[at + 1] >> (7 - x)) & 1) << 1);
                if (!value)
                    continue;
                auto *dest = rgba.data() + ((ty * 8 + y) * 160 + tx * 8 + x) * 4;
                if (portrait) {
                    std::copy_n(palette[value].data(), 3, dest);
                    dest[3] = 255;
                } else
                    pokemon_menu::rgba(dest, colors[value]);
            }
    }
    return battle_graphics.upload(rgba);
}
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
    if (domain == Context::Naming) {
        auto snapshot = naming_menu::prepare(ctx);
        shown.current = shown.selected = ctx->wram[0xc26];
        shown.scroll = 0;
        shown.cursor_x = snapshot.cursor_x;
        shown.cursor_y = snapshot.cursor_y;
        shown.fallback = !naming_menu::draw(ctx, snapshot, layout.text, width, height);
        return;
    }
    if (domain == Context::TrainerCard) {
        auto snapshot = trainer_card::prepare(ctx, trainer_picture);
        if (!snapshot.ready) {
            lcd_overlay::framed(gb_get_framebuffer(ctx), float(width), float(height));
            return;
        }
        std::array<uint8_t, 160 * 144 * 4> rgba{};
        constexpr ImU32 colors[]{0, ui_theme::DimInk, ui_theme::FrameEdge | IM_COL32_A_MASK,
                                 ui_theme::Ink};
        for (int cell = 0; cell < 360; ++cell) {
            if (!snapshot.graphics[size_t(cell)])
                continue;
            int tx = cell % 20, ty = cell / 20, tile = ctx->wram[0x3a0 + cell];
            size_t base = tile < 0x80 ? 0x1000 + tile * 16 : size_t(tile) * 16;
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    int value = ((ctx->vram[base + y * 2] >> (7 - x)) & 1) |
                                (((ctx->vram[base + y * 2 + 1] >> (7 - x)) & 1) << 1);
                    pokemon_menu::rgba(rgba.data() + ((ty * 8 + y) * 160 + tx * 8 + x) * 4,
                                       colors[value]);
                }
        }
        if (!trainer_graphics.upload(rgba)) {
            lcd_overlay::framed(gb_get_framebuffer(ctx), float(width), float(height));
            return;
        }
        auto drawn = menu_text::regions(snapshot.text.data(), ctx->vram, ctx->rom, ctx->rom_size,
                                        gb_get_framebuffer(ctx), layout.text, float(width),
                                        float(height), menu_text::Placement::Full);
        shown.fallback = drawn.panels == 0;
        if (!shown.fallback) {
            int scale = lcd_overlay::framed_scale(float(width), float(height));
            float x = std::floor((width - 160 * scale) / 2.f);
            float y = std::floor((height - 144 * scale) / 2.f);
            ImGui::GetForegroundDrawList()->AddImage((ImTextureID)(intptr_t)trainer_graphics.id,
                                                     {x, y}, {x + 160 * scale, y + 144 * scale});
        }
        return;
    }
    if (domain == Context::Options) {
        auto option = options_menu::selection(ctx);
        shown.current = shown.selected = option.current;
        shown.scroll = 0;
        shown.cursor_x = option.painted >= 0 ? 1 : -1;
        shown.cursor_y = option.painted >= 0 ? 2 + 2 * option.painted : -1;
        if (!option.valid || layout.kind != Kind::Options) {
            lcd_overlay::framed(gb_get_framebuffer(ctx), float(width), float(height));
            return;
        }
        auto drawn = menu_text::regions(ctx->wram + 0x3a0, ctx->vram, ctx->rom, ctx->rom_size,
                                        gb_get_framebuffer(ctx), layout.text, float(width),
                                        float(height), menu_text::Placement::Full);
        shown.fallback = drawn.panels == 0;
        return;
    }
    if (domain == Context::BattleBag) {
        auto snapshot = battle_bag::prepare(ctx, battle_pictures);
        if (!snapshot.ready || !upload_battle_graphics(ctx, snapshot)) {
            lcd_overlay::framed(gb_get_framebuffer(ctx), float(width), float(height));
            return;
        }
        auto drawn = menu_text::regions(snapshot.text.data(), ctx->vram, ctx->rom, ctx->rom_size,
                                        gb_get_framebuffer(ctx), layout.text, float(width),
                                        float(height), menu_text::Placement::Full);
        shown.fallback = drawn.panels == 0;
        if (!shown.fallback) {
            int scale = lcd_overlay::framed_scale(float(width), float(height));
            float x = std::floor((width - 160 * scale) / 2.f);
            float y = std::floor((height - 144 * scale) / 2.f);
            ImGui::GetForegroundDrawList()->AddImage((ImTextureID)(intptr_t)battle_graphics.id,
                                                     {x, y}, {x + 160 * scale, y + 144 * scale});
        }
        return;
    }
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
inline bool draw_options(GBContext *ctx, int width, int height) {
    if (!options_menu::active(ctx))
        return false;
    draw_context(ctx, Context::Options, width, height);
    return true;
}
inline bool draw_trainer(GBContext *ctx, int width, int height) {
    if (!trainer_card::active(ctx))
        return false;
    draw_context(ctx, Context::TrainerCard, width, height);
    return true;
}
inline bool draw_naming(GBContext *ctx, int width, int height) {
    if (!naming_menu::active(ctx))
        return false;
    draw_context(ctx, Context::Naming, width, height);
    return true;
}
} // namespace full_menu
