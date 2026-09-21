#pragma once
#include "imgui.h"
#include <algorithm>

// Shared presentation palette. Legacy colors remain explicit to keep classic
// captures identical while integrated menus adopt the battle HUD tokens.
namespace ui_theme {
struct Color {
    float r, g, b, a = 1;
};
constexpr Color with_alpha(Color color, float alpha) {
    color.a = alpha;
    return color;
}
inline ImU32 opacity(ImU32 color, float alpha) {
    auto a = unsigned(((color >> IM_COL32_A_SHIFT) & 255) * std::clamp(alpha, 0.f, 1.f));
    return (color & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
}
constexpr ImU32 Panel = IM_COL32(17, 29, 33, 238);
constexpr ImU32 Ink = IM_COL32(248, 244, 219, 255);
constexpr ImU32 DimInk = IM_COL32(182, 201, 190, 255);
constexpr ImU32 HpRed = IM_COL32(231, 92, 70, 255);
constexpr ImU32 HpAmber = IM_COL32(237, 192, 71, 255);
constexpr ImU32 HpGreen = IM_COL32(110, 194, 139, 255);
constexpr ImU32 BarTrack = IM_COL32(44, 59, 58, 255);
constexpr ImU32 Experience = IM_COL32(112, 172, 230, 255);
constexpr ImU32 PartyAlive = IM_COL32(222, 109, 84, 255);
constexpr ImU32 PartyFainted = IM_COL32(73, 87, 85, 255);
constexpr ImU32 BattleCaption = IM_COL32(245, 241, 216, 255);
constexpr ImU32 TitleStrip = IM_COL32(250, 238, 205, 225);
constexpr ImU32 Shadow = IM_COL32(0, 0, 0, 35);
constexpr ImU32 Frame = IM_COL32(12, 18, 18, 240);
constexpr ImU32 FrameEdge = IM_COL32(163, 181, 167, 220);
constexpr ImU32 White = IM_COL32(255, 255, 255, 255);
constexpr Color TexturedWhite{1, 1, 1, 1};
constexpr ImU32 Accent = HpAmber;
constexpr float Radius = 6, Padding = 14, BarRadius = 3;
constexpr float CompactPadding = 3, CompactRadius = Radius / 2;
constexpr int StartGlyphScale = 3, BottomGlyphScale = 4;
constexpr int HudGlyphScale = 2, HudDetailScale = 1, HudTitleScale = 3;
constexpr Color InkColor{248.f / 255, 244.f / 255, 219.f / 255};
constexpr Color PanelColor{17.f / 255, 29.f / 255, 33.f / 255};
constexpr Color BallTop{.94f, .94f, .89f};
constexpr Color BallBottom{.86f, .19f, .13f};
constexpr Color BallSeam{.13f, .17f, .19f};
constexpr Color Fire{.99f, .35f, .12f};
constexpr Color Water{.25f, .65f, 1};
constexpr Color Grass{.42f, .86f, .3f};
constexpr Color Electric{1, .89f, .18f};
constexpr Color Psychic{.95f, .4f, .86f};
constexpr Color Ice{.57f, .94f, 1};
constexpr Color MoveNeutral{.81f, .72f, 1};
constexpr Color CaptureSpark{.96f, .93f, .72f};
constexpr Color DexLeft{.66f, .065f, .09f};
constexpr Color DexRight{.76f, .085f, .11f};
constexpr Color DexHinge{.28f, .055f, .065f};
constexpr Color DexBezel{.085f, .12f, .12f};
constexpr Color DexList{.50f, .61f, .53f};
constexpr Color DexScreen{.095f, .17f, .15f};
constexpr Color DexBlueLed{.15f, .69f, .9f};
constexpr Color DexGreenLed{.5f, .83f, .24f};
constexpr Color DexAmberLed{.98f, .77f, .2f};
constexpr Color DexGrille{.23f, .035f, .045f};
constexpr Color ShelfInk{.94f, .93f, .81f};
constexpr Color BoxHeader{.15f, .26f, .25f};
constexpr Color BoxActive{.39f, .37f, .20f};
constexpr Color BoxInactive{.17f, .21f, .23f};
constexpr Color BoxActiveLed{.83f, .73f, .36f};
constexpr Color BoxInactiveLed{.36f, .53f, .52f};
constexpr Color PartyShelf{.16f, .20f, .24f};
constexpr Color BoxSelection{.96f, .77f, .28f};
constexpr Color PcCase{.28f, .34f, .32f};
constexpr Color PcStand{.22f, .27f, .25f};
constexpr Color PcScreen{.17f, .32f, .26f};
constexpr Color PcCounter{.18f, .25f, .23f};
constexpr Color BattleBackground{.10f, .17f, .19f};
constexpr Color DexBackground{.035f, .065f, .075f};
constexpr Color BoxBackground{.035f, .055f, .065f};
constexpr Color Grounds[] = {{.46f, .43f, .31f},
                             {.35f, .47f, .32f},
                             {.23f, .43f, .49f},
                             {.27f, .31f, .32f},
                             {.43f, .42f, .49f}};
} // namespace ui_theme
