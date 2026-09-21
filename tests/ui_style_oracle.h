#pragma once
#include "menu_layout.h"
#include "battle_menu_layout.h"
#include "menu_text.h"
#include "ui_theme.h"
#include "rom_font.h"
#include <SDL_opengles2.h>
#include <vector>

// Independent frame observer: source bits come directly from ROM/VRAM; displayed
// pixels come from the final GL surface, never from a renderer diagnostic copy.
namespace ui_style_qa {
inline bool enabled = false;
inline size_t frames = 0, glyphs = 0, bits = 0, fallback_tiles = 0, cursors = 0;
inline std::array<size_t, 4> battle_frames{};
inline size_t pending_move_cursors = 0;
inline void fail(const char *message, int tile = -1, int x = -1, int y = -1) {
    std::fprintf(stderr, "[UI-STYLE] FAIL %s tile=%02x x=%d y=%d frame=%zu\n", message, tile, x, y,
                 frames);
    std::exit(62);
}
inline void report() {
    if (enabled) {
        std::fprintf(stderr,
                     "[UI-STYLE] frames=%zu glyphs=%zu bits=%zu classic_fallback_tiles=%zu\n",
                     frames, glyphs, bits, fallback_tiles);
        std::fprintf(stderr, "[UI-CURSOR] frames=%zu\n", cursors);
        std::fprintf(stderr, "[UI-MOVE-CURSOR] pending_repaint=%zu\n", pending_move_cursors);
        std::fprintf(stderr, "[UI-BATTLE] message=%zu fight=%zu moves=%zu\n", battle_frames[1],
                     battle_frames[2], battle_frames[3]);
    }
}
inline void observe(GBContext *ctx, int w, int h, bool menu_open) {
    if (!enabled || !ctx || !ctx->wram || menu_open)
        return;
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
