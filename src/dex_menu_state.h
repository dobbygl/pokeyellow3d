#pragma once
#include "dex_state.h"
#include "menu_text.h"

// Pokedex_PlacePokemonList and LoadPokedexTilePatterns, pinned pret e89ead1.
// This profile owns four original text regions. The caught marker is graphic
// 72, loaded from the same ROM asset that Bill's PC loads at 78.
namespace dex_menu {
constexpr std::array<menu_layout::Rect, 4> Regions{
    {{0, 0, 14, 18}, {15, 8, 5, 9}, {16, 1, 4, 2}, {16, 4, 4, 2}}};
struct Snapshot {
    bool ready = false;
    std::array<uint8_t, 360> text{};
    std::array<bool, 7> caught{};
    menu_layout::Layout layout;
};
inline Snapshot sample(const GBContext *ctx, const rom_font::Atlas &font) {
    Snapshot result;
    if (!ctx || !ctx->wram || !ctx->vram || !ctx->rom || !ctx->io || ctx->rom_size != 1048576 ||
        (ctx->io[0x40] & 0x90) != 0x80 || ctx->io[0x47] != 0xe4 || !font.valid)
        return result;
    const auto *tiles = ctx->wram + 0x3a0;
    auto selection = dex_state::list(ctx);
    if (!selection.number || selection.row < 0 || selection.row > 6 ||
        ctx->wram[0xc26] > ctx->wram[0xc28] || ctx->wram[0xc36] > 144)
        return result;
    std::array<bool, 360> owned{};
    result.layout.kind = menu_layout::Kind::Partial;
    result.layout.count = Regions.size();
    std::copy(Regions.begin(), Regions.end(), result.layout.regions.begin());
    std::copy_n(tiles, 360, result.text.begin());
    for (auto r : Regions)
        for (int y = r.y; y < r.y + r.h; ++y)
            for (int x = r.x; x < r.x + r.w; ++x)
                owned[size_t(y * 20 + x)] = true;
    // The complete original separator must exist, with its original patterns.
    for (int tile : {0x70, 0x71, 0x72}) {
        size_t source = tile == 0x72 ? 0x3aa28 : 0x11018 + (tile - 0x60) * 16;
        if (std::memcmp(ctx->vram + 0x1000 + tile * 16, ctx->rom + source, 16))
            return result;
    }
    for (int y = 0; y < 18; ++y)
        for (int x = 0; x < 20; ++x) {
            size_t cell = size_t(y * 20 + x);
            int tile = tiles[cell];
            if (owned[cell]) {
                if (tile == 0x72 && x == 3 && y >= 3 && y <= 15 && y % 2) {
                    result.caught[size_t((y - 3) / 2)] = true;
                    result.text[cell] = 0x7f;
                } else if (tile != 0x7f && (tile < 0x80 || !menu_text::font_matches(font, ctx->vram,
                                                                                    uint8_t(tile))))
                    return result;
            } else if (x == 14) {
                if (tile != ((y == 0 || y % 2) ? 0x71 : 0x70))
                    return result;
            } else if (y == 6 && x >= 15) {
                if (tile != 0x7a)
                    return result;
            } else if (tile != 0x7f)
                return result;
        }
    result.ready = true;
    return result;
}
} // namespace dex_menu
