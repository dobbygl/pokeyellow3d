#pragma once
#include "menu_layout.h"
#include "rom_font.h"
#include <SDL_opengles2.h>
#include <vector>

// Independent frame observer: source bits come directly from ROM/VRAM; displayed
// pixels come from the final GL surface, never from a renderer diagnostic copy.
namespace ui_style_qa {
inline bool enabled = false;
inline size_t frames = 0, glyphs = 0, bits = 0, fallback_tiles = 0;
inline void fail(const char *message, int tile = -1, int x = -1, int y = -1) {
    std::fprintf(stderr, "[UI-STYLE] FAIL %s tile=%02x x=%d y=%d frame=%zu\n", message, tile, x, y,
                 frames);
    std::exit(62);
}
inline void report() {
    if (enabled)
        std::fprintf(stderr,
                     "[UI-STYLE] frames=%zu glyphs=%zu bits=%zu classic_fallback_tiles=%zu\n",
                     frames, glyphs, bits, fallback_tiles);
}
inline void observe(GBContext *ctx, int w, int h, bool menu_open) {
    if (!enabled || !ctx || !ctx->wram || menu_open)
        return;
    auto info = pallet3d_menu();
    auto layout = menu_layout::classify(ctx->wram + 0x3a0);
    if (!info.active || info.full || layout.kind != menu_layout::Kind::Partial ||
        pallet3d_pc().active || pallet3d_blend().active)
        return;
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
    for (int ty = 0; ty < 18; ++ty)
        for (int tx = 0; tx < 20; ++tx) {
            uint8_t tile = ctx->wram[0x3a0 + ty * 20 + tx];
            if (!visible[ty * 20 + tx] || tile < 0x80)
                continue;
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
                    size_t p = (size_t(h - 1 - sy) * w + sx) * 4;
                    uint32_t color = native[lcd_y * 160 + lcd_x];
                    for (int c = 0; c < 3; ++c)
                        if (pixels[p + c] != uint8_t(color >> (16 - 8 * c)))
                            fail("presented classic glyph differs from original LCD", tile, x, y);
                    ++bits;
                }
        }
    if (glGetError() != GL_NO_ERROR)
        fail("GL error reading presented glyphs");
}
} // namespace ui_style_qa
