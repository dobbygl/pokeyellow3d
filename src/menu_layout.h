#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

// Canonical Yellow wTileMap, 20x18 tiles. This recognizes window geometry,
// not translated text or menu selection. Unknown arrangements are full screens.
namespace menu_layout {
struct Rect {
    int x, y, w, h;
};
enum class Kind { None, Partial, Full };
struct Layout {
    Kind kind = Kind::Full;
    std::array<Rect, 8> regions{};
    size_t count = 0;
};
constexpr Rect Bottom{0, 12, 20, 6}, Buy{0, 0, 11, 7}, Money{11, 0, 9, 3};
constexpr Rect Stock{4, 2, 16, 11}, Quantity{7, 9, 13, 3}, YesNo{14, 7, 6, 5};
constexpr Rect SaveCard{4, 0, 16, 10}, SaveYesNo{0, 7, 6, 5};
constexpr Rect HealCancel{11, 6, 9, 6};
constexpr bool contains(Rect r, int x, int y) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}
inline uint8_t border(Rect r, int x, int y) {
    if (y == r.y)
        return x == r.x ? 0x79 : x == r.x + r.w - 1 ? 0x7b : 0x7a;
    if (y == r.y + r.h - 1)
        return x == r.x ? 0x7d : x == r.x + r.w - 1 ? 0x7e : 0x7a;
    return x == r.x || x == r.x + r.w - 1 ? 0x7c : 0;
}
inline bool matches(const uint8_t *tiles, Layout &result, std::initializer_list<Rect> boxes) {
    Layout candidate;
    candidate.kind = Kind::Partial;
    for (auto r : boxes)
        candidate.regions[candidate.count++] = r;
    // Later windows cover earlier borders. Validate every border that remains
    // visible, rather than requiring a hidden edge to survive in wTileMap.
    for (int y = 0; y < 18; y++)
        for (int x = 0; x < 20; x++) {
            int top = -1;
            for (size_t i = 0; i < candidate.count; i++)
                if (contains(candidate.regions[i], x, y))
                    top = int(i);
            uint8_t tile = tiles[y * 20 + x];
            if (top < 0) {
                if (tile >= 0x60)
                    return false;
                continue;
            }
            auto r = candidate.regions[size_t(top)];
            auto expected = border(r, x, y);
            // The money window has its original five-letter title in the top edge.
            if (r.x == Money.x && r.y == 0 && r.w == 9 && r.h == 3 && y == 0 && x >= 13 &&
                x <= 17) {
                constexpr uint8_t title[]{0x8c, 0x8e, 0x8d, 0x84, 0x98};
                expected = title[x - 13];
            }
            if (expected && tile != expected)
                return false;
        }
    result = candidate;
    return true;
}
inline Layout classify(const uint8_t *tiles) {
    Layout result;
    if (!tiles)
        return result;
    bool text = false;
    for (int i = 0; i < 360; i++)
        text |= tiles[i] >= 0x60;
    if (!text) {
        result.kind = Kind::None;
        return result;
    }
    if (matches(tiles, result, {Bottom}))
        return result;
    for (int h : {14, 16}) {
        Rect start{10, 0, 10, h};
        if (matches(tiles, result, {start}) || matches(tiles, result, {start, Bottom}) ||
            matches(tiles, result, {start, Bottom, YesNo}) ||
            matches(tiles, result, {start, SaveCard, Bottom}) ||
            matches(tiles, result, {start, SaveCard, Bottom, SaveYesNo}))
            return result;
    }
    if (matches(tiles, result, {Bottom, YesNo}) || matches(tiles, result, {Bottom, HealCancel}))
        return result;
    if (matches(tiles, result, {Buy, Money, Bottom}) ||
        matches(tiles, result, {Bottom, Buy, Money, Stock}) ||
        matches(tiles, result, {Bottom, Buy, Money, Stock, Quantity}) ||
        matches(tiles, result, {Buy, Money, Stock, Quantity, Bottom}) ||
        matches(tiles, result, {Buy, Money, Stock, Quantity, Bottom, YesNo}))
        return result;
    return result;
}
} // namespace menu_layout
