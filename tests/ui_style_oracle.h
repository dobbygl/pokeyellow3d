#pragma once
#include "menu_layout.h"
#include "battle_menu_layout.h"
#include "menu_text.h"
#include "ui_theme.h"
#include "rom_font.h"
#include "dex_area_state.h"
#include "imgui.h"
#include "ui_full_menu_oracle.h"
#include "ui_pokemon_menu_oracle.h"
#include "ui_dex_menu_oracle.h"
#include <SDL_opengles2.h>
#include <vector>

// Independent frame observer: source bits come directly from ROM/VRAM; displayed
// pixels come from the final GL surface, never from a renderer diagnostic copy.
namespace ui_style_qa {
inline bool enabled = false;
inline size_t frames = 0, glyphs = 0, bits = 0, fallback_tiles = 0, cursors = 0;
inline std::array<size_t, 4> battle_frames{};
inline size_t pending_move_cursors = 0;
inline size_t font_audit_frames = 0, default_glyph_frames = 0;
inline size_t atlas_glyphs = 0, atlas_bits = 0, atlas_occluded_bits = 0;
inline size_t trainer_captions = 0, long_trainer_captions = 0;
inline void fail(const char *message, int tile = -1, int x = -1, int y = -1) {
    std::fprintf(stderr, "[UI-STYLE] FAIL %s tile=%02x x=%d y=%d frame=%zu\n", message, tile, x, y,
                 frames);
    std::exit(62);
}
inline void report() {
    if (enabled) {
        ui_full_menu_qa::report();
        ui_pokemon_qa::report();
        ui_dex_qa::report();
        std::fprintf(stderr,
                     "[UI-STYLE] frames=%zu glyphs=%zu bits=%zu classic_fallback_tiles=%zu\n",
                     frames, glyphs, bits, fallback_tiles);
        std::fprintf(stderr, "[UI-CURSOR] frames=%zu\n", cursors);
        std::fprintf(stderr, "[UI-MOVE-CURSOR] pending_repaint=%zu\n", pending_move_cursors);
        std::fprintf(stderr, "[UI-FONT] gameplay_frames=%zu default_glyph_frames=%zu\n",
                     font_audit_frames, default_glyph_frames);
        std::fprintf(stderr, "[UI-ATLAS] glyphs=%zu bits=%zu lcd_occluded_bits=%zu\n", atlas_glyphs,
                     atlas_bits, atlas_occluded_bits);
        std::fprintf(stderr, "[UI-BATTLE] message=%zu fight=%zu moves=%zu\n", battle_frames[1],
                     battle_frames[2], battle_frames[3]);
        std::fprintf(stderr, "[UI-TRAINER] captions=%zu long_names=%zu\n", trainer_captions,
                     long_trainer_captions);
    }
}
inline bool default_font_glyphs(const ImDrawData *data) {
    if (!data)
        return false;
    auto *atlas = ImGui::GetIO().Fonts;
    for (int list = 0; list < data->CmdListsCount; ++list) {
        const auto *dl = data->CmdLists[list];
        for (const auto &cmd : dl->CmdBuffer) {
            if (cmd.UserCallback || cmd.GetTexID() != atlas->TexID)
                continue;
            for (unsigned i = 0; i + 2 < cmd.ElemCount; i += 3) {
                auto uv = [&](int n) {
                    return dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + i + n]].uv;
                };
                auto a = uv(0), b = uv(1), c = uv(2);
                float u0 = std::min({a.x, b.x, c.x}), u1 = std::max({a.x, b.x, c.x});
                float v0 = std::min({a.y, b.y, c.y}), v1 = std::max({a.y, b.y, c.y});
                // Solid shapes use one white texel; anti-aliased lines use
                // their own strip. Neither is a glyph in the font atlas.
                if (u0 == u1 || v0 == v1)
                    continue;
                for (const auto *font : atlas->Fonts)
                    for (const auto &glyph : font->Glyphs)
                        if (glyph.Visible && u0 >= glyph.U0 && u1 <= glyph.U1 && v0 >= glyph.V0 &&
                            v1 <= glyph.V1)
                            return true;
            }
        }
    }
    return false;
}
inline void observe_atlas(GBContext *ctx, int w, int h) {
    const auto *data = ImGui::GetDrawData();
    unsigned texture = pallet3d_font_texture();
    auto arena = pallet3d_battle();
    if (!data || !texture || pallet3d_blend().active ||
        (arena.active && (arena.full_overlay || arena.overlay_alpha > 0)))
        return;
    // wTrainerName occupies D049..D055: twelve glyphs plus its terminator.
    // Count expected cells independently; checking only submitted quads would
    // miss a truncated suffix even if every remaining glyph had correct bits.
    std::array<bool, 13> trainer_cells{};
    size_t trainer_length = 0;
    if (arena.active && arena.trainer_class)
        while (trainer_length < trainer_cells.size() && ctx->wram[0x1049 + trainer_length] != 0x50)
            ++trainer_length;
    // AREA UNKNOWN is a native LCD window drawn over the map labels. Only
    // recognize that exact submitted quad; verify its pixels instead of the
    // hidden glyph bits. All visible portions of each label remain checked.
    int area_scale = std::max(1, std::min(w / 160, 3));
    int area_left = int(std::floor((w - 136 * area_scale) * .5f));
    int area_top = int(std::floor((h - 32 * area_scale) * .5f));
    bool area_window = false;
    static std::array<uint32_t, 160 * 144> area_lcd{};
    static int area_species = -1;
    static bool area_lcd_valid = false;
    auto area = pallet3d_area();
    if (!area.active || area.species != area_species) {
        area_lcd_valid = false;
        area_species = area.species;
    }
    if (area.active && dex_area_state::title_ready(ctx, gb_get_framebuffer(ctx))) {
        // The renderer retains the last complete LCD while leaving AREA.
        // Observe that source frame independently, before it is overwritten.
        std::copy_n(gb_get_framebuffer(ctx), area_lcd.size(), area_lcd.begin());
        area_lcd_valid = true;
    }
    if (area.active && area.maps == 0)
        for (int list = 0; list < data->CmdListsCount; ++list) {
            const auto *dl = data->CmdLists[list];
            for (const auto &cmd : dl->CmdBuffer) {
                if (cmd.UserCallback || cmd.ElemCount % 6)
                    continue;
                for (unsigned i = 0; i < cmd.ElemCount; i += 6) {
                    const auto &a = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + i]];
                    const auto &b =
                        dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + i + 2]];
                    area_window |= a.pos.x == area_left && a.pos.y == area_top &&
                                   b.pos.x == area_left + 136 * area_scale &&
                                   b.pos.y == area_top + 32 * area_scale && a.uv.x == 1.f / 20 &&
                                   a.uv.y == 7.f / 18 && b.uv.x == 18.f / 20 && b.uv.y == 11.f / 18;
                }
            }
        }
    std::vector<uint8_t> pixels;
    for (int list = 0; list < data->CmdListsCount; ++list) {
        const auto *dl = data->CmdLists[list];
        for (const auto &cmd : dl->CmdBuffer) {
            if (cmd.UserCallback || cmd.GetTexID() != (ImTextureID)(intptr_t)texture)
                continue;
            if (pixels.empty()) {
                pixels.resize(size_t(w) * h * 4);
                glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            }
            if (cmd.ElemCount % 6)
                fail("ROM atlas draw is not a sequence of glyph quads");
            for (unsigned i = 0; i < cmd.ElemCount; i += 6) {
                const auto &a = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + i]];
                const auto &b = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + i + 2]];
                int tile = 128 + int(std::lround(a.uv.y * 8)) * 16 + int(std::lround(a.uv.x * 16));
                if (tile < 128 || tile > 255 || b.pos.x <= a.pos.x || b.pos.y <= a.pos.y)
                    fail("invalid ROM atlas glyph quad");
                if ((a.col >> IM_COL32_A_SHIFT) != 255)
                    fail("ROM glyph is not fully visible");
                if (trainer_length && a.pos.y == 24 && a.pos.x >= 24 &&
                    a.pos.x < 24 + float(trainer_cells.size() * 16)) {
                    size_t cell = size_t(std::lround((a.pos.x - 24) / 16));
                    if (cell >= trainer_length || trainer_cells[cell] ||
                        a.pos.x != 24 + float(cell * 16) || b.pos.x - a.pos.x != 16 ||
                        b.pos.y - a.pos.y != 16 || tile != ctx->wram[0x1049 + cell])
                        fail("trainer caption differs from its original name buffer", tile);
                    trainer_cells[cell] = true;
                }
                for (int y = 0; y < 8; ++y)
                    for (int x = 0; x < 8; ++x) {
                        float sx = a.pos.x + (x + .5f) * (b.pos.x - a.pos.x) / 8;
                        float sy = a.pos.y + (y + .5f) * (b.pos.y - a.pos.y) / 8;
                        if (sx < cmd.ClipRect.x || sy < cmd.ClipRect.y || sx >= cmd.ClipRect.z ||
                            sy >= cmd.ClipRect.w)
                            continue;
                        int px = int((sx - data->DisplayPos.x) * data->FramebufferScale.x);
                        int py = int((sy - data->DisplayPos.y) * data->FramebufferScale.y);
                        if (px < 0 || py < 0 || px >= w || py >= h)
                            fail("ROM atlas glyph outside viewport", tile, px, py);
                        size_t at = (size_t(h - 1 - py) * w + px) * 4;
                        if (area_window && px >= area_left && py >= area_top &&
                            px < area_left + 136 * area_scale && py < area_top + 32 * area_scale) {
                            int lx = 8 + (px - area_left) / area_scale;
                            int ly = 56 + (py - area_top) / area_scale;
                            if (!area_lcd_valid)
                                fail("AREA window has no observed native source frame");
                            uint32_t lcd = area_lcd[ly * 160 + lx];
                            if (pixels[at] != uint8_t(lcd >> 16) ||
                                pixels[at + 1] != uint8_t(lcd >> 8) ||
                                pixels[at + 2] != uint8_t(lcd))
                                fail("AREA window occlusion differs from native LCD", tile, px, py);
                            ++atlas_occluded_bits;
                            continue;
                        }
                        bool ink = pixels[at] == uint8_t(a.col >> IM_COL32_R_SHIFT) &&
                                   pixels[at + 1] == uint8_t(a.col >> IM_COL32_G_SHIFT) &&
                                   pixels[at + 2] == uint8_t(a.col >> IM_COL32_B_SHIFT);
                        bool expected =
                            (ctx->rom[rom_font::Offset + (tile - 128) * 8 + y] >> (7 - x)) & 1;
                        if (ink != expected)
                            fail("drawn ROM atlas glyph differs from cartridge bits", tile, px, py);
                        ++atlas_bits;
                    }
                ++atlas_glyphs;
            }
        }
    }
    if (trainer_length) {
        for (size_t cell = 0; cell < trainer_length; ++cell)
            if (trainer_cells[cell] != (ctx->wram[0x1049 + cell] >= 0x80))
                fail("trainer caption omits an original name glyph", ctx->wram[0x1049 + cell],
                     int(cell));
        ++trainer_captions;
        long_trainer_captions += trainer_length > 10;
    }
}
inline void observe(GBContext *ctx, int w, int h, bool menu_open) {
    if (!enabled || !ctx || !ctx->wram || menu_open)
        return;
    if (pallet3d_menu_style() == ui_preferences::Style::Integrated) {
        ++font_audit_frames;
        if (default_font_glyphs(ImGui::GetDrawData())) {
            ++default_glyph_frames;
            fail("default ImGui glyph drawn during integrated gameplay");
        }
        observe_atlas(ctx, w, h);
        ui_full_menu_qa::observe(ctx, w, h);
        ui_pokemon_qa::observe(ctx, w, h);
        ui_dex_qa::observe(ctx, w, h);
    }
    auto info = pallet3d_menu();
    auto arena = pallet3d_battle();
    bool in_battle = arena.active && arena.integrated_menu;
    auto battle_layout = battle_menu::classify(ctx->wram + 0x3a0);
    auto layout = in_battle ? battle_layout.regions : menu_layout::classify(ctx->wram + 0x3a0);
    if ((!in_battle && (!info.active || info.full)) || layout.kind != menu_layout::Kind::Partial ||
        pallet3d_pc().active || pallet3d_blend().active)
        return;
    if (in_battle) {
        if (arena.full_overlay || arena.overlay_alpha != 0 ||
            arena.menu_kind != int(battle_layout.kind) || !arena.menu_kind)
            fail("integrated battle menu overlaps full LCD fallback");
        ++battle_frames[size_t(arena.menu_kind)];
    }
    ++frames;
    const auto *native = gb_get_framebuffer(ctx);
    int scale = std::max(1, int(std::floor(std::min(w / 160.f, h / 144.f))));
    int left = (w - 160 * scale) / 2, top = h - 144 * scale;
    std::vector<uint8_t> pixels(size_t(w) * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    std::array<bool, 360> visible{};
    for (size_t i = 0; i < layout.count; ++i) {
        const auto &r = layout.regions[i];
        for (int y = r.y; y < r.y + r.h; ++y)
            for (int x = r.x; x < r.x + r.w; ++x)
                visible[y * 20 + x] = true;
    }
    const auto &font = rom_font::get(ctx->rom, ctx->rom_size);
    const bool integrated = pallet3d_menu_style() == ui_preferences::Style::Integrated;
    auto plan =
        menu_text::prepare(ctx->wram + 0x3a0, ctx->vram, font, layout, float(w), float(h),
                           ui_theme::Padding, ui_theme::StartGlyphScale, ui_theme::BottomGlyphScale,
                           in_battle ? menu_text::Placement::Battle : menu_text::Placement::World);
    if (in_battle && battle_layout.kind == battle_menu::Kind::Moves) {
        // battle3d lifts TYPE/PP above the move list; the glyph checks below
        // follow that placement. Independently require the visible gap.
        menu_text::separate(plan, battle_menu::MovesInfo, battle_menu::MovesList,
                            ui_theme::Padding);
        const auto &info_panel = plan.panels[battle_menu::MovesInfo];
        const auto &list_panel = plan.panels[battle_menu::MovesList];
        if (info_panel.visible && list_panel.visible && info_panel.right > list_panel.left &&
            info_panel.bottom + ui_theme::Padding > list_panel.top)
            fail("TYPE/PP panel overlaps the integrated move list");
    }
    if (in_battle) {
        int expected_panels = 0;
        for (size_t i = 0; i < layout.count; ++i) {
            bool ink = false, fallback = false;
            for (int cell = 0; cell < 360; ++cell) {
                if (plan.owner[cell] != int(i))
                    continue;
                uint8_t tile = ctx->wram[0x3a0 + cell];
                fallback |= tile < 0x79;
                if (tile < 0x80)
                    continue;
                for (int row = 0; row < 8; ++row) {
                    uint8_t pattern = ctx->rom[rom_font::Offset + (tile - 128) * 8 + row];
                    ink |= pattern != 0;
                    for (int plane = 0; plane < 2; ++plane)
                        fallback |=
                            ctx->vram[0x800 + (tile - 128) * 16 + row * 2 + plane] != pattern;
                }
            }
            expected_panels += ink && !fallback;
        }
        if (arena.menu_panels != expected_panels)
            fail("battle draws an empty panel or omits a populated one");
    }
    for (int ty = 0; ty < 18; ++ty)
        for (int tx = 0; tx < 20; ++tx) {
            uint8_t tile = ctx->wram[0x3a0 + ty * 20 + tx];
            if (!visible[ty * 20 + tx] || tile < 0x80)
                continue;
            // Addresses and one/two-row spacing verified in pokeyellow_internal.h
            // and the original PlaceMenuCursor routine. Old unfilled arrows in
            // covered menus are content too, but only this pointer is active.
            int cursor_address = ctx->wram[0xc30] | (ctx->wram[0xc31] << 8);
            if (tile == 0xed && cursor_address == 0xc3a0 + ty * 20 + tx) {
                int step = ctx->hram[0x7a] & 2 ? 1 : 2;
                int item = ctx->wram[0xc26];
                // SelectMenuItem clears the spacing flag after HandleMenuInput,
                // then updates the selection/PP window before repainting the
                // arrow. PlaceMenuCursor records the displayed item in CC2A.
                // During this interval compare against that original cursor,
                // not the next logical selection. Its pixels are still checked.
                bool pending = in_battle && battle_layout.kind == battle_menu::Kind::Moves &&
                               !(ctx->hram[0x7a] & 2);
                if (pending) {
                    step = 1;
                    item = ctx->wram[0xc2a];
                }
                if (tx != ctx->wram[0xc25] || ty != ctx->wram[0xc24] + step * item) {
                    std::fprintf(
                        stderr,
                        "[UI-CURSOR] top=%u,%u item=%u flags=%02x battle=%d kind=%d cycles=%llu\n",
                        ctx->wram[0xc25], ctx->wram[0xc24], ctx->wram[0xc26], ctx->hram[0x7a],
                        in_battle, int(battle_layout.kind), (unsigned long long)ctx->cycles);
                    gb_context_save_state_file(ctx, "logs/cursor-disagreement.state");
                    fail("active cursor disagrees with original menu selection", tile, tx, ty);
                }
                if (pending)
                    ++pending_move_cursors;
                else
                    ++cursors;
            }
            bool original_font = true;
            for (int y = 0; y < 8; ++y)
                for (int plane = 0; plane < 2; ++plane)
                    original_font &= ctx->vram[0x800 + (tile - 128) * 16 + y * 2 + plane] ==
                                     ctx->rom[rom_font::Offset + (tile - 128) * 8 + y];
            if (!original_font)
                ++fallback_tiles;
            ++glyphs;
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    bool expected =
                        (ctx->rom[rom_font::Offset + (tile - 128) * 8 + y] >> (7 - x)) & 1;
                    if (font.ink(tile, x, y) != expected)
                        fail("atlas bit differs from ROM", tile, x, y);
                    int lcd_x = tx * 8 + x, lcd_y = ty * 8 + y;
                    int sx = left + lcd_x * scale + scale / 2;
                    int sy = top + lcd_y * scale + scale / 2;
                    bool styled = false;
                    if (integrated) {
                        // Independently find the original topmost owner; the renderer
                        // must not draw this glyph again in a covered lower window.
                        int owner = -1;
                        for (size_t i = 0; i < layout.count; ++i)
                            if (menu_layout::contains(layout.regions[i], tx, ty))
                                owner = int(i);
                        if (owner < 0 || plan.owner[ty * 20 + tx] != owner)
                            fail("incorrect source ownership", tile, tx, ty);
                        const auto &panel = plan.panels[size_t(owner)];
                        sx = int(panel.origin_x) + lcd_x * panel.scale + panel.scale / 2;
                        sy = int(panel.origin_y) + lcd_y * panel.scale + panel.scale / 2;
                        styled = !panel.fallback;
                        if (styled && !original_font)
                            fail("replaced VRAM font did not fall back", tile, x, y);
                    }
                    if (sx < 0 || sx >= w || sy < 0 || sy >= h)
                        fail("glyph outside viewport", tile, sx, sy);
                    size_t p = (size_t(h - 1 - sy) * w + sx) * 4;
                    if (styled) {
                        bool ink = pixels[p] == 248 && pixels[p + 1] == 244 && pixels[p + 2] == 219;
                        if (ink != expected)
                            fail("presented integrated glyph differs from ROM", tile, x, y);
                    } else {
                        uint32_t color = native[lcd_y * 160 + lcd_x];
                        for (int c = 0; c < 3; ++c)
                            if (pixels[p + c] != uint8_t(color >> (16 - 8 * c)))
                                fail("presented classic glyph differs from original LCD", tile, x,
                                     y);
                    }
                    ++bits;
                }
        }
    if (glGetError() != GL_NO_ERROR)
        fail("GL error reading presented glyphs");
}
} // namespace ui_style_qa
