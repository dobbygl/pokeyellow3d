#pragma once
#include "menu_layout.h"

// Complete-screen composition is opt-in and requires a verified engine context.
// Geometry below is from pret/pokeyellow e89ead154b9968aa50eed9328ff2b38b6c194382:
// DisplayPCMainMenu, PlayerPCMenu, BillsPCMenu, DisplayDepositWithdrawMenu,
// DisplayListMenuID, DisplayChooseQuantityMenu and ChangeBox. The renderer must
// still validate each visible glyph/graphic before replacing the original LCD.
namespace full_menu {
using menu_layout::Rect;
enum class Context { None, PcCenter, PcItems, PcBill, PcOak };
enum class Kind { Unknown, PcMain, PcList, PcAction, PcBoxes, PcDialogue };
struct Layout {
    Kind kind = Kind::Unknown;
    menu_layout::Layout text;
};
struct ListSnapshot {
    int current = 0, scroll = 0, selected = 0;
    int cursor_x = -1, cursor_y = -1;
};
inline ListSnapshot list_snapshot(const uint8_t *wram) {
    ListSnapshot result;
    if (!wram)
        return result;
    // Original wCurrentMenuItem, wListScrollOffset and wMenuCursorLocation.
    // Current+scroll is the logical list entry; the painted arrow remains the
    // authority while the engine repaints a row or returns from a child menu.
    result.current = wram[0xc26];
    result.scroll = wram[0xc36];
    result.selected = result.current + result.scroll;
    int cell = (wram[0xc30] | (wram[0xc31] << 8)) - 0xc3a0;
    if (cell >= 0 && cell < 360 && wram[0x3a0 + cell] == 0xed) {
        result.cursor_x = cell % 20;
        result.cursor_y = cell / 20;
    }
    return result;
}
constexpr Rect BillMain{0, 0, 14, 14}, BillBox{9, 14, 11, 4};
constexpr Rect List{4, 2, 16, 11}, Action{9, 10, 11, 8}, Quantity{15, 9, 5, 3};
constexpr Rect BoxNumber{0, 0, 11, 4}, BoxList{11, 0, 9, 14};

inline Layout classify(const uint8_t *tiles, Context context) {
    Layout result;
    if (!tiles || context == Context::None)
        return result;
    auto matches = [&](Kind kind, std::initializer_list<Rect> boxes) {
        if (!menu_layout::matches(tiles, result.text, boxes))
            return false;
        result.kind = kind;
        return true;
    };
    constexpr auto bottom = menu_layout::Bottom, yes_no = menu_layout::YesNo;
    if (context == Context::PcBill) {
        if (matches(Kind::PcBoxes, {BillMain, bottom, BoxNumber, BoxList}) ||
            matches(Kind::PcAction, {bottom, BillMain, BillBox, List, Action}) ||
            matches(Kind::PcAction, {BillMain, List, bottom, yes_no}) ||
            matches(Kind::PcAction, {bottom, BillMain, BillBox, List, yes_no}) ||
            matches(Kind::PcList, {bottom, BillMain, BillBox, List}) ||
            matches(Kind::PcList, {BillMain, List, bottom}) ||
            matches(Kind::PcMain, {bottom, BillMain, BillBox}) ||
            matches(Kind::PcDialogue, {BillMain, bottom, yes_no}) ||
            matches(Kind::PcDialogue, {BillMain, bottom}))
            return result;
    } else {
        // Before the Pokedex, after receiving it, and after the League, the
        // original terminal menu has respectively three, four and five rows.
        for (int height : {8, 10, 12}) {
            if (context == Context::PcItems && height != 10)
                continue;
            Rect main{0, 0, 16, height};
            if (context == Context::PcItems &&
                (matches(Kind::PcAction, {main, List, Quantity, bottom, yes_no}) ||
                 matches(Kind::PcAction, {main, List, Quantity, bottom}) ||
                 matches(Kind::PcAction, {main, bottom, List, Quantity}) ||
                 matches(Kind::PcAction, {main, List, bottom, yes_no}) ||
                 matches(Kind::PcList, {main, bottom, List}) ||
                 matches(Kind::PcList, {main, List, bottom})))
                return result;
            if (matches(Kind::PcDialogue, {main, bottom, yes_no}) ||
                matches(Kind::PcMain, {main, bottom}) || matches(Kind::PcMain, {main}))
                return result;
        }
    }
    if (matches(Kind::PcDialogue, {bottom, yes_no}) || matches(Kind::PcDialogue, {bottom}))
        return result;
    return {};
}
} // namespace full_menu
