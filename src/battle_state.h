#pragma once
#include "gbrt.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>

// Canonical UE Yellow. Addresses checked against pokeyellow_internal.h and
// pret/pokeyellow's symbols, engine/battle/{core,init_battle,animations}.asm.
// These helpers only read machine state; presentation owns all interpolation.
namespace battle {
constexpr int TileMap = 0xc3a0, IsInBattle = 0xd056, BattleType = 0xd059, Link = 0xd12a;
constexpr int Animation = 0xd07b, AnimDelay = 0xd085, AnimCounter = 0xd086;
struct Rect {
    int x, y, base;
};
// CopyUncompressedPicToHL writes seven columns of seven consecutive tiles.
// LoadMonBackPic scales the 28px back image to 56px before loading vBackPic.
constexpr Rect Enemy{12, 0, 0}, Player{1, 5, 0x31};
constexpr int PortraitSize = 56;
using Image = std::array<uint8_t, PortraitSize * PortraitSize * 4>;
inline uint64_t fingerprint(const Image &image) {
    uint64_t value = 14695981039346656037ull;
    for (uint8_t byte : image) {
        value ^= byte;
        value *= 1099511628211ull;
    }
    return value;
}
inline uint8_t read(const GBContext *ctx, int a) {
    return ctx->wram[a - 0xc000];
}
inline int word(const GBContext *ctx, int a) {
    return read(ctx, a) * 256 + read(ctx, a + 1);
}
inline uint8_t tile(const GBContext *ctx, int x, int y) {
    return read(ctx, TileMap + y * 20 + x);
}
inline bool normal(const GBContext *ctx) {
    return ctx && ctx->wram && ctx->vram && ctx->io && ctx->rom && ctx->rom_size == 1048576 &&
           read(ctx, IsInBattle) >= 1 && read(ctx, IsInBattle) <= 2 && !read(ctx, BattleType) &&
           !read(ctx, Link);
}
inline bool rectangle(const GBContext *ctx, Rect r, bool displayed = false) {
    for (int y = 0; y < 7; y++)
        for (int x = 0; x < 7; x++) {
            int id = r.base + x * 7 + y;
            if (tile(ctx, r.x + x, r.y + y) != id)
                return false;
            if (displayed) {
                // Battle UI is normally the LCD window at (WX=7,WY=0), not BG0.
                bool window = (ctx->io[0x40] & 0x20) && ctx->io[0x4a] == 0 && ctx->io[0x4b] == 7;
                int map = (ctx->io[0x40] & (window ? 0x40 : 8)) ? 0x1c00 : 0x1800;
                if (ctx->vram[map + (r.y + y) * 32 + r.x + x] != id)
                    return false;
            }
        }
    return true;
}
// True while the LCD layer that presents the battle shows exactly these
// wTileMap cells. Effect animations shake the window (WX/WY) or scroll the BG
// by less than a tile; the same tiles remain visible, only displaced. A BG map
// that has not yet received wTileMap (auto BG transfer pending), another
// selected map, a disabled BG or an off LCD all show something else.
inline bool displayed(const GBContext *ctx, int x0, int y0, int w, int h) {
    if (!ctx || !ctx->wram || !ctx->vram || !ctx->io || x0 < 0 || y0 < 0 || w <= 0 || h <= 0 ||
        x0 + w > 20 || y0 + h > 18)
        return false;
    uint8_t lcdc = ctx->io[0x40];
    if ((lcdc & 0x81) != 0x81)
        return false;
    bool window = (lcdc & 0x20) && ctx->io[0x4a] < 8 && ctx->io[0x4b] < 15;
    int map = (lcdc & (window ? 0x40 : 8)) ? 0x1c00 : 0x1800;
    int ox = window ? 0 : ctx->io[0x43] / 8, oy = window ? 0 : ctx->io[0x42] / 8;
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++)
            if (ctx->vram[map + ((y + oy) & 31) * 32 + ((x + ox) & 31)] != tile(ctx, x, y))
                return false;
    return true;
}
inline bool name_matches(const GBContext *ctx, int address, int x, int y) {
    // A HUD name must be present, not the trainer-introduction picture or a
    // full-screen party/item list reusing the same species bytes.
    int length = 0;
    while (length < 10 && read(ctx, address + length) != 0x50)
        ++length;
    // CenterMonName shifts names shorter than five characters right by 1–2.
    x += length <= 2 ? 2 : length <= 4 ? 1 : 0;
    int count = 0;
    for (; count < 3; count++) {
        int c = read(ctx, address + count);
        if (c == 0x50)
            break;
        if (c < 0x80 || tile(ctx, x + count, y) != c)
            return false;
    }
    return count > 0;
}
inline bool animation_running(const GBContext *ctx);
inline bool trainer_intro(const GBContext *ctx) {
    // InitBattle sets EnemyMonPartyPos to FF while the trainer owns vFrontPic.
    // LoadEnemyMonData replaces it before sending the first Pokemon. This is
    // stronger than FirstMonsNotOutYet, which also covers Spearow's entrance.
    return normal(ctx) && read(ctx, IsInBattle) == 2 && read(ctx, 0xd030) > 0 &&
           read(ctx, 0xcfe7) == 255 && (ctx->io[0x40] & 0x80) && ctx->io[0x47] == 0xe4 &&
           !ctx->io[0x42] && (ctx->io[0x43] == 0 || ctx->io[0x43] == 2) &&
           rectangle(ctx, Enemy, true);
}
inline bool ready(const GBContext *ctx) {
    if (trainer_intro(ctx))
        return true;
    if (!normal(ctx) || !(ctx->io[0x40] & 0x80) || read(ctx, 0xd11c) || !read(ctx, 0xcfe4) ||
        !read(ctx, 0xd013) || !word(ctx, 0xcff3) || !word(ctx, 0xd022))
        return false;
    // Animations deliberately erase portraits, scroll the LCD and flash the
    // palette. Their verified lifetime lets the compositor retain the arena
    // and either play a supported effect or show the complete original LCD.
    if (animation_running(ctx))
        return true;
    return ctx->io[0x47] == 0xe4 && ctx->io[0x42] == 0 &&
           (ctx->io[0x43] == 0 || ctx->io[0x43] == 2) && !read(ctx, 0xd11c) && read(ctx, 0xcfe4) &&
           read(ctx, 0xd013) && word(ctx, 0xcff3) && word(ctx, 0xd022) && rectangle(ctx, Enemy) &&
           name_matches(ctx, 0xcfd9, 1, 0) && name_matches(ctx, 0xd008, 10, 7);
}
inline bool live_return(const GBContext *ctx, size_t call, int target) {
    // CALL and its conditional forms (C4/CC/D4/DC) push the same return
    // address; a taken conditional call is as live as an unconditional one.
    int op = ctx->rom[call];
    if ((op != 0xcd && op != 0xc4 && op != 0xcc && op != 0xd4 && op != 0xdc) ||
        ctx->rom[call + 1] != (target & 255) || ctx->rom[call + 2] != (target >> 8))
        return false;
    if (ctx->sp < 0xd000 || ctx->sp >= 0xe000)
        return false;
    int address = int(call % 0x4000) + 0x4003;
    for (int a = ctx->sp; a < 0xdfff; a += 2)
        if (read(ctx, a) == (address & 255) && read(ctx, a + 1) == (address >> 8))
            return true;
    return false;
}
inline bool capture_running(const GBContext *ctx) {
    return normal(ctx) && read(ctx, IsInBattle) == 1 && live_return(ctx, 0x79fe7, 0x4124);
}
inline bool animation_running(const GBContext *ctx) {
    if (!normal(ctx) || !read(ctx, Animation))
        return false;
    // IDs/counters are unions and persist. Verified live CALL returns cover
    // PlayMoveAnimation, direct MoveAnimation calls, its damage feedback, and
    // each stage of TossBallAnimation, even during a nested audio bank call.
    return live_return(ctx, 0x3f09b, 0x3eb4) || live_return(ctx, 0x3f0a3, 0x3e84) ||
           live_return(ctx, 0x78dbc, 0x4124) || live_return(ctx, 0x78dc6, 0x4df6) ||
           capture_running(ctx);
}
// A far routine entered through Bankswitch (0x3e84) runs above the frame
// [0x3e90][F][caller bank][caller return]: Bankswitch pushes AF with A = the
// caller's bank, then CALLs JumpToAddress, whose return is 0x3e90. This also
// identifies a far routine reached by `jp Bankswitch`, which leaves no CALL of
// its own; the caller bank removes the 16-bit ambiguity of live_return.
inline bool far_frame(const GBContext *ctx, int bank, int ret) {
    if (ctx->sp < 0xd000 || ctx->sp >= 0xe000)
        return false;
    for (int a = ctx->sp; a + 5 <= 0xdfff; a += 2)
        if (read(ctx, a) == 0x90 && read(ctx, a + 1) == 0x3e && read(ctx, a + 3) == bank &&
            read(ctx, a + 4) == (ret & 255) && read(ctx, a + 5) == (ret >> 8))
            return true;
    return false;
}
// Engine phase of a normal battle, identified only by live engine frames
// (engine/battle/core.asm, experience.asm, pokemon/learn_move.asm,
// items/item_effects.asm, movie/evolution.asm). Subphases whose original
// layout is not presented yet (stats box, yes/no, lists, party balls) are
// separate values so presentation can keep them on the complete LCD.
enum class Phase {
    None,         // not in a normal battle, or outside StartBattle/EndOfBattle
    TrainerIntro, // StartBattle -> EnemySendOutFirstMon, first send-out
    Turn,         // MainInBattleLoop and anything more specific not listed
    Fainted,      // FaintEnemyPokemon / RemoveFaintedPlayerMon
    Experience,   // GainExperience: "gained N EXP. Points!"
    LevelUp,      // GainExperience: HUD redraw and "grew to level N!"
    LevelStats,   // GainExperience: PrintStatsBox and its wait
    LearnMove,    // LearnMoveFromLevelUp messages
    LearnYesNo,   // LearnMove: yes/no boxes
    LearnForget,  // LearnMove: list of moves to forget
    Switch,       // next Pokemon of either side, retreat and send-out
    SwitchYesNo,  // "Use next POKéMON?" or "Will ... change POKéMON?"
    SwitchParty,  // party list reached from a switch
    PartyBalls,   // ReplaceFaintedEnemyMon drawing the enemy's party balls
    TrainerOutro, // TrainerBattleVictory
    PlayerDefeat, // HandlePlayerBlackOut
    Run,          // TryRunningFromBattle / EnemyRan
    Capture,      // ItemUseBall, its Pokedex entry, nickname and storage
    Evolution,    // EndOfBattle -> EvolutionAfterBattle
    Exit,         // EndOfBattle without evolution
};
inline const char *phase_name(Phase phase) {
    static const char *names[] = {"None",        "TrainerIntro", "Turn",         "Fainted",
                                  "Experience",  "LevelUp",      "LevelStats",   "LearnMove",
                                  "LearnYesNo",  "LearnForget",  "Switch",       "SwitchYesNo",
                                  "SwitchParty", "PartyBalls",   "TrainerOutro", "PlayerDefeat",
                                  "Run",         "Capture",      "Evolution",    "Exit"};
    return names[int(phase)];
}
inline bool any_return(const GBContext *ctx, std::initializer_list<std::pair<size_t, int>> pairs) {
    for (auto p : pairs)
        if (live_return(ctx, p.first, p.second))
            return true;
    return false;
}
inline Phase phase(const GBContext *ctx) {
    if (!normal(ctx))
        return Phase::None;
    // _InitBattleCommon farcalls StartBattle (0f:4127) at F613F and then
    // EndOfBattle (04:7765) at F6147. Either frame is the root of every phase.
    auto far = [&](size_t call, int target, int bank) {
        return ctx->rom[call - 5] == 0x21 && ctx->rom[call - 4] == (target & 255) &&
               ctx->rom[call - 3] == (target >> 8) && ctx->rom[call - 2] == 0x06 &&
               ctx->rom[call - 1] == bank && live_return(ctx, call, 0x3e84);
    };
    if (far(0xf6147, 0x7765, 0x04))
        return live_return(ctx, 0x137d0, 0x3eb4) ? Phase::Evolution : Phase::Exit;
    if (!far(0xf613f, 0x4127, 0x0f))
        return Phase::None;
    // ItemUseBall (bank 3): its animation, result texts, Pokedex entry,
    // AddPartyMon/SendNewMonToBox; wCapturedMonSpecies stays set until the
    // bag returns after a successful capture.
    if (read(ctx, 0xd11b) || any_return(ctx, {{0x79fe7, 0x4124},
                                              {0xd606, 0x3c36},
                                              {0xd632, 0x3c36},
                                              {0xd640, 0x3eb4},
                                              {0xd661, 0x391c},
                                              {0xd669, 0x66e8},
                                              {0xd679, 0x3c36},
                                              {0xd681, 0x3c36}}))
        return Phase::Capture;
    // GainExperience (15:525F) runs inside FaintEnemyPokemon: by farcall at
    // 3C633 (EXP.ALL first pass) or by `jp Bankswitch` at 3C651, which leaves
    // the Bankswitch frame of FaintEnemyPokemon's caller (bank 0F, returns
    // 4542 from HandleEnemyMonFainted or 4737 from HandlePlayerMonFainted).
    bool experience = live_return(ctx, 0x3c633, 0x3e84) || far_frame(ctx, 0x0f, 0x4542) ||
                      far_frame(ctx, 0x0f, 0x4737);
    if (experience) {
        // LearnMoveFromLevelUp (predef 1A) from GainExperience, then the
        // shared LearnMove (bank 1) boxes.
        if (live_return(ctx, 0x5542f, 0x3eb4)) {
            if (any_return(ctx, {{0x6c9e, 0x3010}, {0x6c70, 0x3010}}))
                return Phase::LearnYesNo;
            if (live_return(ctx, 0x6cfd, 0x3aab))
                return Phase::LearnForget;
            return Phase::LearnMove;
        }
        if (any_return(ctx, {{0x5541a, 0x3e84}, {0x5541d, 0x3852}}))
            return Phase::LevelStats;
        if (any_return(ctx, {{0x55409, 0x3c36},
                             {0x553de, 0x54c1},
                             {0x553e4, 0x54c1},
                             {0x553ea, 0x54c1},
                             {0x553f0, 0x54c1},
                             {0x553f6, 0x54c1}}))
            return Phase::LevelUp;
        return Phase::Experience;
    }
    if (any_return(ctx,
                   {{0x3c53f, 0x457d}, {0x3c734, 0x457d}, {0x3c722, 0x475e}, {0x3c5fe, 0x475e}}))
        return Phase::Fainted;
    // DoUseNextMonDialogue, ChooseNextMon, ReplaceFaintedEnemyMon (which
    // falls into EnemySendOutFirstMon and jumps to SwitchPlayerMon), the AI's
    // SwitchEnemyMon/EnemySendOut, and the voluntary RetreatMon/SendOutMon.
    bool use_next = any_return(ctx, {{0x3c742, 0x47ff}, {0x3c564, 0x47ff}});
    bool choose = any_return(ctx, {{0x3c746, 0x483c}, {0x3c568, 0x483c}});
    bool replace = any_return(ctx, {{0x3c570, 0x467a}, {0x3c751, 0x467a}});
    if (use_next || choose || replace ||
        any_return(ctx, {{0x3c2f8, 0x3e84},
                         {0x3a808, 0x3e84},
                         {0x3d2c6, 0x3e84},
                         {0x3d2ce, 0x4d97},
                         {0x3d2ef, 0x4cfb}})) {
        if ((use_next && live_return(ctx, 0x3c81c, 0x3010)) || live_return(ctx, 0x3ca4d, 0x3010))
            return Phase::SwitchYesNo;
        if ((choose && any_return(ctx, {{0x3c841, 0x11c8}, {0x3c846, 0x11dd}})) ||
            any_return(ctx, {{0x3ca5b, 0x11c8}, {0x3ca74, 0x11dd}, {0x3ca71, 0x3c36}}))
            return Phase::SwitchParty;
        if (replace && live_return(ctx, 0x3c693, 0x3e84))
            return Phase::PartyBalls;
        return Phase::Switch;
    }
    if (any_return(ctx, {{0x3c6d9, 0x4710},
                         {0x3c6df, 0x3c36},
                         {0x3c6e8, 0x6e9e},
                         {0x3c6ed, 0x372f},
                         {0x3c6f0, 0x331d},
                         {0x3c6f6, 0x3c36}}))
        return Phase::TrainerOutro;
    if (any_return(ctx, {{0x3c8b3, 0x6e9e},
                         {0x3c8b8, 0x372f},
                         {0x3c8be, 0x3c36},
                         {0x3c8d9, 0x3c36},
                         {0x3c8e4, 0x16dd}}))
        return Phase::PlayerDefeat;
    if (any_return(ctx, {{0x3d30f, 0x4b1e},
                         {0x3cbb9, 0x3c36},
                         {0x3cbf0, 0x3736},
                         {0x3cbf6, 0x3c36},
                         {0x3c22c, 0x3c36}}))
        return Phase::Run;
    if (live_return(ctx, 0x3c14d, 0x498f) || trainer_intro(ctx))
        return Phase::TrainerIntro;
    return Phase::Turn;
}
// The screen outside the bottom message box, as the covered phases leave it:
// each side either shows its HUD (its name at the CenterMonName position) or
// has cleared it, and its picture rectangle holds only blank cells or tiles of
// a picture (a partial slide or send-out stage). Every other cell of rows 0-11
// is blank. A stats box, yes/no box, list, party balls or any other window
// breaks one of these, so the caller keeps the complete original LCD.
constexpr int EnemyHud[4] = {0, 0, 11, 4}, PlayerHud[4] = {9, 7, 11, 5};
inline bool present(const GBContext *ctx, bool enemy) {
    return enemy ? name_matches(ctx, 0xcfd9, 1, 0) : name_matches(ctx, 0xd008, 10, 7);
}
inline bool arena_coherent(const GBContext *ctx) {
    if (!normal(ctx) || !(ctx->io[0x40] & 0x80) || ctx->io[0x47] != 0xe4 || ctx->io[0x42] ||
        (ctx->io[0x43] != 0 && ctx->io[0x43] != 2) || read(ctx, 0xd11c))
        return false;
    bool shown[2] = {present(ctx, false), present(ctx, true)};
    auto inside = [](const int *zone, int x, int y) {
        return x >= zone[0] && x < zone[0] + zone[2] && y >= zone[1] && y < zone[1] + zone[3];
    };
    for (int y = 0; y < 12; y++)
        for (int x = 0; x < 20; x++) {
            int t = tile(ctx, x, y);
            // Text box borders and the menu cursor never belong to the HUD.
            if ((t >= 0x79 && t <= 0x7e) || t == 0xed || t == 0xec)
                return false;
            bool handled = false;
            for (int side = 0; side < 2 && !handled; side++) {
                Rect r = side ? Enemy : Player;
                if (x >= r.x && x < r.x + 7 && y >= r.y && y < r.y + 7) {
                    // AnimateSendingOutMon's small stages borrow picture
                    // tiles of either side; HUD, box and text tiles are >= 62.
                    if (t != 0x7f && t >= Player.base + 49)
                        return false;
                    handled = true;
                }
            }
            for (int side = 0; side < 2 && !handled; side++)
                if (inside(side ? EnemyHud : PlayerHud, x, y)) {
                    if (!shown[side] && t != 0x7f)
                        return false;
                    handled = true;
                }
            if (!handled && t != 0x7f)
                return false;
        }
    return true;
}
// Phases B1 presents over the arena when arena_coherent() confirms them.
inline bool held_phase(Phase phase) {
    return phase == Phase::Fainted || phase == Phase::Experience || phase == Phase::LevelUp ||
           phase == Phase::LearnMove || phase == Phase::Switch;
}
// A switch is held only while the player's own Pokemon stays on screen: the
// enemy's replacement. The player's retreat and send-out ("Come back!",
// "Go!") replace the player's HUD and picture through animations that keep
// the complete LCD until they are presented in 3D (phase C1).
inline bool holds(const GBContext *ctx, Phase phase) {
    return held_phase(phase) && arena_coherent(ctx) &&
           (phase != Phase::Switch || (present(ctx, false) && rectangle(ctx, Player)));
}
enum class Effect { Original, Physical, Projectile, Status, Self };
struct Move {
    int id = 0, type = 0, effect = 0, power = 0;
    Effect presentation = Effect::Original;
};
inline Move move(const GBContext *ctx, int id) {
    if (id < 1 || id > 165)
        return {};
    size_t p = 0x38000 + 6 * (id - 1);
    if (ctx->rom[p] != id)
        return {};
    Move m{id, ctx->rom[p + 3], ctx->rom[p + 1], ctx->rom[p + 2]};
    int e = m.effect;
    if (!m.power) {
        if ((e >= 0x0a && e <= 0x0f) || (e >= 0x32 && e <= 0x37) || e == 0x2e || e == 0x2f ||
            e == 0x38 || e == 0x40 || e == 0x41)
            m.presentation = Effect::Self;
        else if ((e >= 0x12 && e <= 0x17) || (e >= 0x3a && e <= 0x3f) || e == 0x20 || e == 0x31 ||
                 e == 0x42 || e == 0x43 || e == 0x54 || e == 0x56)
            m.presentation = Effect::Status;
    } else {
        // Multi-turn, transforming, trapping, OHKO and other special protocols
        // keep the complete original animation. Simple damage and side effects
        // use Gen I's ROM type split, without altering damage calculation.
        switch (e) {
        case 0:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 0x10:
        case 0x11:
        case 0x1f:
        case 0x21:
        case 0x22:
        case 0x24:
        case 0x25:
        case 0x30:
        case 0x44:
        case 0x45:
        case 0x46:
        case 0x47:
        case 0x4c:
            m.presentation = m.type < 20 ? Effect::Physical : Effect::Projectile;
            break;
        }
    }
    return m;
}
inline std::string text(const GBContext *ctx, int address) {
    std::string value;
    for (int i = 0; i < 10; i++) {
        int c = read(ctx, address + i);
        if (c == 0x50)
            break;
        if (c >= 0x80 && c <= 0x99)
            value += char('A' + c - 0x80);
        else if (c >= 0xa0 && c <= 0xb9)
            value += char('a' + c - 0xa0);
        else if (c >= 0xf6)
            value += char('0' + c - 0xf6);
        else if (c == 0x7f)
            value += ' ';
        else if (c == 0xe3)
            value += '-';
        else if (c == 0xef)
            value += "M";
        else if (c == 0xf5)
            value += "F";
        else if (c == 0xe8)
            value += '.';
        else if (c == 0xe0)
            value += '\'';
        else
            value += '?';
    }
    return value;
}
struct Mon {
    int species, hp, max_hp, level, status, hp_color;
    std::string name;
};
inline Mon mon(const GBContext *ctx, bool enemy) {
    int b = enemy ? 0xcfe4 : 0xd013;
    return {read(ctx, b),
            word(ctx, b + 1),
            word(ctx, b + 15),
            read(ctx, b + 14),
            read(ctx, b + 4),
            read(ctx, enemy ? 0xcf1d : 0xcf1c),
            text(ctx, enemy ? 0xcfd9 : 0xd008)};
}
inline const char *status(int s) {
    if (s & 7)
        return "SLP";
    if (s & 8)
        return "PSN";
    if (s & 16)
        return "BRN";
    if (s & 32)
        return "FRZ";
    if (s & 64)
        return "PAR";
    return "";
}
inline int dex(const GBContext *ctx, int species) {
    if (species < 1 || species > 190)
        return 0;
    int number = ctx->rom[0x410b1 + species - 1];
    return number <= 151 ? number : 0;
}
inline std::array<std::array<uint8_t, 3>, 4> palette(const GBContext *ctx, int species) {
    std::array<std::array<uint8_t, 3>, 4> out{
        {{255, 255, 255}, {170, 170, 170}, {85, 85, 85}, {0, 0, 0}}};
    int n = dex(ctx, species);
    if (!n)
        return out;
    int id = ctx->rom[0x72921 + n];
    if (id >= 40)
        return out;
    // CGBBasePalettes, little endian RGB555, eight bytes per palette.
    for (int i = 0; i < 4; i++) {
        size_t p = 0x72af9 + id * 8 + i * 2;
        int c = ctx->rom[p] | (ctx->rom[p + 1] << 8);
        for (int k = 0; k < 3; k++)
            out[i][k] = uint8_t(((c >> (k * 5)) & 31) * 255 / 31);
    }
    return out;
}
inline Image portrait(const GBContext *ctx, Rect r, int species) {
    Image out{};
    std::array<uint8_t, 56 * 56> indices{};
    auto colors = palette(ctx, species);
    for (int y = 0; y < 56; y++)
        for (int x = 0; x < 56; x++) {
            int id = tile(ctx, r.x + x / 8, r.y + y / 8);
            int at = (ctx->io[0x40] & 16) ? id * 16 : 0x1000 + int(int8_t(id)) * 16;
            int row = at + (y % 8) * 2, bit = 7 - x % 8;
            int v = ((ctx->vram[row] >> bit) & 1) | (((ctx->vram[row + 1] >> bit) & 1) << 1);
            int i = y * 56 + x;
            indices[i] = uint8_t(v);
            for (int c = 0; c < 3; c++)
                out[i * 4 + c] = colors[v][c];
            out[i * 4 + 3] = 255;
        }
    // Only remove background white connected to the outside. White eyes and
    // highlights enclosed by the outline remain opaque.
    if (r.base == Player.base) {
        // Back portraits are cropped at the bottom of the LCD picture. Close
        // that crop between its first and last ink pixel before the flood fill,
        // otherwise the white torso is mistaken for outside background.
        int left = 56, right = -1;
        for (int x = 0; x < 56; x++)
            if (indices[55 * 56 + x]) {
                left = std::min(left, x);
                right = x;
            }
        for (int x = left; x <= right; x++)
            if (!indices[55 * 56 + x])
                indices[55 * 56 + x] = 4;
    }
    std::array<int, 56 * 56> queue{};
    int head = 0, tail = 0;
    auto push = [&](int i) {
        if (!indices[i] && out[i * 4 + 3]) {
            out[i * 4 + 3] = 0;
            queue[tail++] = i;
        }
    };
    for (int i = 0; i < 56; i++) {
        push(i);
        push(55 * 56 + i);
        push(i * 56);
        push(i * 56 + 55);
    }
    while (head < tail) {
        int i = queue[head++], x = i % 56, y = i / 56;
        if (x)
            push(i - 1);
        if (x < 55)
            push(i + 1);
        if (y)
            push(i - 56);
        if (y < 55)
            push(i + 56);
    }
    return out;
}
inline int experience_at(const GBContext *ctx, int species, int level) {
    int n = dex(ctx, species);
    if (!n)
        return -1;
    int growth = ctx->rom[0x383de + (n - 1) * 28 + 19];
    if (growth > 5)
        return -1;
    size_t p = 0x58e73 + growth * 4;
    int a = ctx->rom[p] >> 4, b = ctx->rom[p] & 15;
    if (!b)
        return -1;
    int c = ctx->rom[p + 1];
    c = (c & 128) ? -(c & 127) : c;
    return std::max(0, a * level * level * level / b + c * level * level + ctx->rom[p + 2] * level -
                           ctx->rom[p + 3]);
}
inline float experience(const GBContext *ctx) {
    int slot = read(ctx, 0xcc2f);
    if (slot >= read(ctx, 0xd162) || slot >= 6)
        return 0;
    int p = 0xd16a + slot * 44, species = read(ctx, p), level = read(ctx, p + 33);
    if (level >= 100)
        return 1;
    int value = (read(ctx, p + 14) << 16) | (read(ctx, p + 15) << 8) | read(ctx, p + 16);
    int a = experience_at(ctx, species, level), b = experience_at(ctx, species, level + 1);
    return a >= 0 && b > a ? std::clamp(float(value - a) / (b - a), 0.f, 1.f) : 0;
}
} // namespace battle
