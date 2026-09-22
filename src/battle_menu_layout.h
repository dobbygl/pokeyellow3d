#pragma once
#include "menu_layout.h"
#include <algorithm>
#include <array>

// Original battle windows only. Portraits and status bars are deliberately
// outside this classifier; battle3d still owns their full-screen fallback.
namespace battle_menu {
enum class Kind { Unknown, Message, Fight, Moves };
struct Layout {
    Kind kind = Kind::Unknown;
    menu_layout::Layout regions;
};
constexpr menu_layout::Rect Fight{8, 12, 12, 6}, Moves{4, 12, 16, 6}, MoveInfo{0, 8, 11, 5};
// Region order of Kind::Moves in classify(): Bottom, Moves, MoveInfo. The
// original TYPE/PP window shares its bottom edge row with the move list's top
// edge, so its integrated panel must be lifted clear of the four move rows.
constexpr size_t MovesList = 1, MovesInfo = 2;
inline bool matches(const uint8_t *tiles, menu_layout::Layout &out,
                    std::initializer_list<menu_layout::Rect> boxes) {
    std::array<uint8_t, 360> windows{};
    for (auto r : boxes)
        for (int y = r.y; y < r.y + r.h; ++y)
            for (int x = r.x; x < r.x + r.w; ++x)
                windows[y * 20 + x] = tiles[y * 20 + x];
    return menu_layout::matches(windows.data(), out, boxes);
}
inline Layout classify(const uint8_t *tiles) {
    Layout result;
    if (!tiles)
        return result;
    if (battle_menu::matches(tiles, result.regions, {menu_layout::Bottom, Moves, MoveInfo}))
        result.kind = Kind::Moves;
    else if (battle_menu::matches(tiles, result.regions, {menu_layout::Bottom, Fight}))
        result.kind = Kind::Fight;
    else if (battle_menu::matches(tiles, result.regions, {menu_layout::Bottom}))
        result.kind = Kind::Message;
    return result;
}
} // namespace battle_menu
