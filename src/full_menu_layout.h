#pragma once
#include "menu_layout.h"

// Complete-screen composition is opt-in and requires a verified engine context.
// Geometry below is from pret/pokeyellow e89ead154b9968aa50eed9328ff2b38b6c194382:
// DisplayPCMainMenu, PlayerPCMenu, BillsPCMenu, DisplayDepositWithdrawMenu,
// DisplayListMenuID, DisplayChooseQuantityMenu and ChangeBox. The renderer must
// still validate each visible glyph/graphic before replacing the original LCD.
namespace full_menu {
using menu_layout::Rect;
enum class Context {
    None,
    PcCenter,
    PcItems,
    PcBill,
    PcOak,
    Bag,
    Mart,
    BattleBag,
    Options,
    TrainerCard,
    Naming
};
enum class Kind {
    Unknown,
    PcMain,
    PcList,
    PcAction,
    PcBoxes,
    PcDialogue,
    BagList,
    BagAction,
    Mart,
    BattleBag,
    Options,
    TrainerCard,
    Naming,
    Count
};
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
    if (context == Context::Naming) {
        constexpr Rect keyboard{0, 4, 20, 11};
        for (int y = 4; y <= 14; ++y)
            for (int x = 0; x < 20; ++x) {
                int expected = menu_layout::border(keyboard, x, y);
                if (expected && tiles[y * 20 + x] != expected)
                    return {};
            }
        result.kind = Kind::Naming;
        result.text.kind = menu_layout::Kind::Partial;
        result.text.count = 3;
        result.text.regions[0] = {0, 0, 20, 4};
        result.text.regions[1] = keyboard;
        result.text.regions[2] = {0, 15, 20, 3};
        return result;
    }
    if (context == Context::TrainerCard) {
        // TrainerInfo_DrawTextBox uses a distinct right/bottom edge. The middle
        // label and side background also belong to this complete source page.
        for (auto r : {Rect{0, 0, 20, 8}, Rect{1, 10, 18, 8}})
            for (int y = r.y; y < r.y + r.h; ++y)
                for (int x = r.x; x < r.x + r.w; ++x) {
                    int edge = menu_layout::border(r, x, y);
                    if (edge == 0x7a && y == r.y + r.h - 1)
                        edge = 0x77;
                    if (edge == 0x7c && x == r.x + r.w - 1)
                        edge = 0x78;
                    if (edge && tiles[y * 20 + x] != edge)
                        return {};
                }
        result.kind = Kind::TrainerCard;
        result.text.kind = menu_layout::Kind::Partial;
        result.text.count = 3;
        result.text.regions[0] = {0, 0, 20, 8};
        result.text.regions[1] = {0, 8, 20, 2};
        result.text.regions[2] = {0, 10, 20, 8};
        return result;
    }
    if (context == Context::Options) {
        if (matches(Kind::Options, {{0, 0, 20, 18}}))
            return result;
        return {};
    }
    if (context == Context::BattleBag) {
        // The original battle keeps HUD and portrait fragments outside these
        // windows. Their stricter graphic profile is validated before drawing.
        std::array<uint8_t, 360> windows{};
        for (auto r : {bottom, List})
            for (int y = r.y; y < r.y + r.h; ++y)
                for (int x = r.x; x < r.x + r.w; ++x)
                    windows[size_t(y * 20 + x)] = tiles[y * 20 + x];
        if (!menu_layout::matches(windows.data(), result.text, {bottom, List}))
            return {};
        result.kind = Kind::BattleBag;
        result.text.count = 3;
        result.text.regions[0] = {0, 0, 20, 18};
        result.text.regions[1] = bottom;
        result.text.regions[2] = List;
        return result;
    }
    if (context == Context::Bag) {
        constexpr Rect action{13, 10, 7, 5};
        for (int height : {14, 16}) {
            Rect start{10, 0, 10, height};
            if (matches(Kind::BagAction, {start, List, action, Quantity, bottom, yes_no}) ||
                matches(Kind::BagAction, {start, List, action, Quantity, bottom}) ||
                matches(Kind::BagAction, {start, List, action, Quantity}) ||
                matches(Kind::BagAction, {start, List, action, bottom, yes_no}) ||
                matches(Kind::BagAction, {start, List, action, bottom}) ||
                matches(Kind::BagAction, {start, List, action}) ||
                matches(Kind::BagList, {start, List}))
                return result;
        }
        return {};
    }
    if (context == Context::Mart) {
        // An unsellable key item overlays the list with a message without
        // first opening a quantity window (original mart .unsellableItem).
        if (matches(Kind::Mart, {menu_layout::Buy, menu_layout::Money, List, bottom}))
            return result;
        // Original buy/sell lists use the same layered windows as the partial
        // mart. Present all surviving regions on the shared complete-screen grid.
        auto original = menu_layout::classify(tiles);
        if (original.kind == menu_layout::Kind::Partial)
            return {Kind::Mart, original};
        return {};
    }
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
