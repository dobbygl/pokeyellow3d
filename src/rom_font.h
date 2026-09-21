#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

// Original 1 bpp FontGraphics. No character substitutions or rasterization.
// One shared decoded atlas also supplies the legacy PC texture subrectangles,
// preserving their exact UVs and filtering in classic mode.
namespace rom_font {
constexpr size_t Offset = 0x10600, Glyphs = 128, Width = 128, Height = 64;
struct Atlas {
    bool valid = false;
    std::array<uint8_t, Glyphs * 8> rows{};
    std::array<uint8_t, Width * Height * 4> rgba{};
    bool ink(uint8_t tile, int x, int y) const {
        return valid && tile >= 0x80 && x >= 0 && x < 8 && y >= 0 && y < 8 &&
               (rows[(tile - 0x80) * 8 + y] & (0x80 >> x));
    }
};
inline Atlas decode(const uint8_t *rom, size_t size) {
    Atlas result;
    if (!rom || size < Offset + result.rows.size())
        return result;
    std::copy_n(rom + Offset, result.rows.size(), result.rows.begin());
    result.valid = true;
    for (int glyph = 0; glyph < int(Glyphs); ++glyph)
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                size_t dest = ((glyph / 16 * 8 + y) * Width + glyph % 16 * 8 + x) * 4;
                result.rgba[dest] = result.rgba[dest + 1] = result.rgba[dest + 2] = 255;
                result.rgba[dest + 3] = result.ink(uint8_t(glyph + 0x80), x, y) ? 255 : 0;
            }
    return result;
}
inline Atlas atlas;
inline size_t decodes = 0;
inline const Atlas &get(const uint8_t *rom, size_t size) {
    static const Atlas invalid;
    if (!rom || size < Offset + atlas.rows.size())
        return invalid;
    if (!atlas.valid || std::memcmp(rom + Offset, atlas.rows.data(), atlas.rows.size())) {
        atlas = decode(rom, size);
        ++decodes;
    }
    return atlas;
}
inline bool copy_to(const uint8_t *rom, size_t size, uint8_t *rgba, size_t width, size_t height,
                    size_t x, size_t y) {
    if (!rom || size < Offset + Glyphs * 8 || !rgba || width < Width || height < Height ||
        x > width - Width || y > height - Height)
        return false;
    const auto &font = get(rom, size);
    if (!font.valid)
        return false;
    for (size_t row = 0; row < Height; ++row)
        std::copy_n(font.rgba.data() + row * Width * 4, Width * 4,
                    rgba + ((row + y) * width + x) * 4);
    return true;
}
} // namespace rom_font
