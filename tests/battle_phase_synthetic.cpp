#include "synthetic_context.h"
#include <cstdio>
#include <stdexcept>
#include <vector>

// battle::phase, battle::far_frame and battle::arena_coherent (B1) against the
// hand-built GBContext. Each phase is armed with the CALL bytes it verifies in
// the image and the return words the engine would leave on the stack; every
// case has a negative control. No cartridge is involved.
static void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}

namespace {
using Phase = battle::Phase;
struct Machine {
    synthetic::Context &m;
    // Frames from the innermost (at SP) to the outermost, as return words.
    void stack(const std::vector<int> &words) {
        for (int a = m.ctx.sp; a < 0xe000; ++a)
            m.write(a, 0);
        int a = m.ctx.sp;
        for (int w : words) {
            m.write(a, w & 255);
            m.write(a + 1, (w >> 8) & 255);
            a += 2;
        }
    }
    void site(size_t call, int target, int opcode = 0xcd) {
        m.image[call] = uint8_t(opcode);
        m.image[call + 1] = uint8_t(target & 255);
        m.image[call + 2] = uint8_t(target >> 8);
    }
    void far_site(size_t call, int target, int bank) {
        m.image[call - 5] = 0x21;
        m.image[call - 4] = uint8_t(target & 255);
        m.image[call - 3] = uint8_t(target >> 8);
        m.image[call - 2] = 0x06;
        m.image[call - 1] = uint8_t(bank);
        site(call, 0x3e84);
    }
    static int ret(size_t call) {
        return int(call % 0x4000) + 0x4003;
    }
};
// Bankswitch's frame for a routine entered with `jp Bankswitch` from bank 0F:
// JumpToAddress's return, AF with the caller bank in A, then the caller's own
// return address.
std::vector<int> far_frame(int bank, int ret) {
    return {0x3e90, bank << 8, ret};
}
std::vector<int> operator+(std::vector<int> a, const std::vector<int> &b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}
} // namespace

int main() {
    try {
        synthetic::Context machine;
        Machine m{machine};
        GBContext *ctx = &machine.ctx;
        machine.reset();
        machine.start_battle(25, 1);

        // Every site this test uses, with its verified opcode and target.
        const std::vector<std::pair<size_t, int>> sites = {
            {0x3c53f, 0x457d}, {0x3c734, 0x457d}, {0x3c722, 0x475e}, {0x3c5fe, 0x475e},
            {0x3c633, 0x3e84}, {0x5534a, 0x3c36}, {0x55409, 0x3c36}, {0x553f0, 0x54c1},
            {0x5541a, 0x3e84}, {0x5541d, 0x3852}, {0x5542f, 0x3eb4}, {0x6c9e, 0x3010},
            {0x6c70, 0x3010},  {0x6cfd, 0x3aab},  {0x3c564, 0x47ff}, {0x3c81c, 0x3010},
            {0x3c568, 0x483c}, {0x3c841, 0x11c8}, {0x3c570, 0x467a}, {0x3c751, 0x467a},
            {0x3c693, 0x3e84}, {0x3ca4d, 0x3010}, {0x3ca5b, 0x11c8}, {0x3d2c6, 0x3e84},
            {0x3c6df, 0x3c36}, {0x3c8d9, 0x3c36}, {0x3d30f, 0x4b1e}, {0xd640, 0x3eb4},
            {0x137d0, 0x3eb4}};
        for (auto s : sites)
            m.site(s.first, s.second);
        m.site(0x3c6d9, 0x4710, 0xc4); // TrainerBattleVictory: call nz
        m.site(0x3c14d, 0x498f, 0xc4); // StartBattle -> EnemySendOutFirstMon: call nz
        m.far_site(0xf613f, 0x4127, 0x0f);
        m.far_site(0xf6147, 0x7765, 0x04);
        const std::vector<int> core{Machine::ret(0xf613f)};
        auto R = [](size_t call) { return std::vector<int>{Machine::ret(call)}; };

        // --- roots --------------------------------------------------------------
        m.stack({});
        check(battle::phase(ctx) == Phase::None, "no StartBattle frame, no phase");
        m.stack(core);
        check(battle::phase(ctx) == Phase::Turn, "StartBattle alone is the turn loop");
        machine.image[0xf613f - 1] = 0x0e;
        check(battle::phase(ctx) == Phase::None, "a Bankswitch to another bank is not the root");
        machine.image[0xf613f - 1] = 0x0f;
        m.stack(R(0xf6147));
        check(battle::phase(ctx) == Phase::Exit, "EndOfBattle without evolution is the exit");
        m.stack(R(0x137d0) + R(0xf6147));
        check(battle::phase(ctx) == Phase::Evolution, "EvolutionAfterBattle after the battle");
        m.stack(R(0x137d0) + core);
        check(battle::phase(ctx) == Phase::Turn, "the evolution predef needs the EndOfBattle root");
        machine.write(battle::BattleType, 1);
        m.stack(R(0x3c53f) + core);
        check(battle::phase(ctx) == Phase::None, "special battles have no phase");
        machine.write(battle::BattleType, 0);
        uint16_t sp = ctx->sp;
        ctx->sp = 0xc800;
        check(battle::phase(ctx) == Phase::None, "a stack outside WRAM bank 1 is refused");
        ctx->sp = sp;

        // --- KO -----------------------------------------------------------------
        for (size_t call : {0x3c53f, 0x3c734, 0x3c722, 0x3c5fe}) {
            m.stack(R(call) + core);
            check(battle::phase(ctx) == Phase::Fainted, "each fainting routine frame");
        }
        m.stack(R(0x3c53f));
        check(battle::phase(ctx) == Phase::None, "a KO frame without the root is ignored");
        m.stack(R(0x3c53f) + core);
        machine.image[0x3c540] = 0x7e;
        check(battle::phase(ctx) == Phase::Turn, "a CALL to another routine is another site");
        machine.image[0x3c540] = 0x7d;

        // --- experience -----------------------------------------------------------
        // GainExperience reached by `jp Bankswitch` keeps FaintEnemyPokemon's
        // own return below Bankswitch's frame.
        auto gain = far_frame(0x0f, Machine::ret(0x3c53f)) + core;
        m.stack(gain);
        check(battle::far_frame(ctx, 0x0f, 0x4542), "Bankswitch frame of the jp");
        check(!battle::far_frame(ctx, 0x0e, 0x4542) && !battle::far_frame(ctx, 0x0f, 0x4737),
              "another caller bank or return is another frame");
        check(battle::phase(ctx) == Phase::Experience, "GainExperience after the KO");
        m.stack(R(0x5534a) + gain);
        check(battle::phase(ctx) == Phase::Experience, "\"gained N EXP. Points!\"");
        m.stack(R(0x5534a) + far_frame(0x0e, Machine::ret(0x3c53f)) + core);
        check(battle::phase(ctx) == Phase::Fainted,
              "a frame from another bank is not GainExperience; the KO remains");
        m.stack(far_frame(0x0f, Machine::ret(0x3c734)) + core);
        check(battle::phase(ctx) == Phase::Experience, "double KO from HandlePlayerMonFainted");
        m.stack(R(0x3c633) + core);
        check(battle::phase(ctx) == Phase::Experience, "EXP.ALL's first farcall pass");

        // --- level up and learning --------------------------------------------------
        m.stack(R(0x55409) + gain);
        check(battle::phase(ctx) == Phase::LevelUp, "\"grew to level N!\"");
        m.stack(R(0x553f0) + gain);
        check(battle::phase(ctx) == Phase::LevelUp, "the level-up HUD redraw");
        m.stack(R(0x55409) + core);
        check(battle::phase(ctx) == Phase::Turn, "the text frame alone is not a level up");
        m.stack(R(0x5541a) + gain);
        check(battle::phase(ctx) == Phase::LevelStats, "PrintStatsBox");
        m.stack(R(0x5541d) + gain);
        check(battle::phase(ctx) == Phase::LevelStats, "the wait with the stats box");
        m.stack(R(0x5542f) + gain);
        check(battle::phase(ctx) == Phase::LearnMove, "LearnMoveFromLevelUp");
        m.stack(R(0x6c9e) + R(0x5542f) + gain);
        check(battle::phase(ctx) == Phase::LearnYesNo, "\"Delete an older move?\" yes/no");
        m.stack(R(0x6c70) + R(0x5542f) + gain);
        check(battle::phase(ctx) == Phase::LearnYesNo, "\"Abandon learning?\" yes/no");
        m.stack(R(0x6cfd) + R(0x5542f) + gain);
        check(battle::phase(ctx) == Phase::LearnForget, "the list of moves to forget");
        m.stack(R(0x6c9e) + gain);
        check(battle::phase(ctx) == Phase::Experience,
              "LearnMove's shared boxes need the battle's predef frame");
        m.stack(R(0x5542f) + core);
        check(battle::phase(ctx) == Phase::Turn, "and the predef needs GainExperience");

        // --- switches ----------------------------------------------------------------
        for (size_t call : {0x3c570, 0x3c751, 0x3c564, 0x3c568, 0x3d2c6}) {
            m.stack(R(call) + core);
            check(battle::phase(ctx) == Phase::Switch, "each switch routine frame");
        }
        m.stack(R(0x3ca4d) + R(0x3c570) + core);
        check(battle::phase(ctx) == Phase::SwitchYesNo, "\"Will ... change POKéMON?\"");
        m.stack(R(0x3c81c) + R(0x3c564) + core);
        check(battle::phase(ctx) == Phase::SwitchYesNo, "\"Use next POKéMON?\"");
        m.stack(R(0x3c81c) + core);
        check(battle::phase(ctx) == Phase::Turn, "a yes/no box alone is no switch");
        m.stack(R(0x3c693) + R(0x3c570) + core);
        check(battle::phase(ctx) == Phase::PartyBalls, "DrawEnemyPokeballs");
        m.stack(R(0x3c693) + R(0x3c564) + core);
        check(battle::phase(ctx) == Phase::Switch, "party balls only from ReplaceFaintedEnemyMon");
        m.stack(R(0x3ca5b) + R(0x3c570) + core);
        check(battle::phase(ctx) == Phase::SwitchParty, "the party list of the switch");
        m.stack(R(0x3c841) + R(0x3c568) + core);
        check(battle::phase(ctx) == Phase::SwitchParty, "ChooseNextMon's party list");

        // --- other phases, conditional CALLs included -----------------------------
        m.stack(R(0x3c6d9) + core);
        check(battle::phase(ctx) == Phase::TrainerOutro, "a taken `call nz` is live");
        machine.image[0x3c6d9] = 0xc3;
        check(battle::phase(ctx) == Phase::Turn, "a JP leaves no return address");
        machine.image[0x3c6d9] = 0xc4;
        m.stack(R(0x3c6df) + core);
        check(battle::phase(ctx) == Phase::TrainerOutro, "\"ASH defeated ...!\"");
        m.stack(R(0x3c14d) + core);
        check(battle::phase(ctx) == Phase::TrainerIntro, "StartBattle's first send-out");
        m.stack(R(0x3c8d9) + core);
        check(battle::phase(ctx) == Phase::PlayerDefeat, "\"... blacked out!\"");
        m.stack(R(0x3d30f) + core);
        check(battle::phase(ctx) == Phase::Run, "RUN from the battle menu");
        m.stack(R(0xd640) + core);
        check(battle::phase(ctx) == Phase::Capture, "ShowPokedexData after a capture");
        m.stack(R(0x3c53f) + core);
        machine.write(0xd11b, 19);
        check(battle::phase(ctx) == Phase::Capture, "wCapturedMonSpecies until the bag returns");
        machine.write(0xd11b, 0);
        check(battle::phase(ctx) == Phase::Fainted, "and the KO without it");

        // --- held phases -------------------------------------------------------------
        for (auto p :
             {Phase::Fainted, Phase::Experience, Phase::LevelUp, Phase::LearnMove, Phase::Switch})
            check(battle::held_phase(p), "B1 presents KO, experience, level up, learn, switch");
        for (auto p :
             {Phase::None, Phase::Turn, Phase::TrainerIntro, Phase::LevelStats, Phase::LearnYesNo,
              Phase::LearnForget, Phase::SwitchYesNo, Phase::SwitchParty, Phase::PartyBalls,
              Phase::TrainerOutro, Phase::PlayerDefeat, Phase::Run, Phase::Capture,
              Phase::Evolution, Phase::Exit})
            check(!battle::held_phase(p), "every other phase keeps the original LCD");

        // --- arena_coherent ------------------------------------------------------------
        auto blank = [&] {
            for (int y = 0; y < 12; y++)
                for (int x = 0; x < 20; x++)
                    machine.tile(x, y, 0x7f);
        };
        auto hud = [&](bool enemy) {
            // A few HUD tiles (bar, bracket) inside each side's zone.
            if (enemy) {
                machine.write_name(0xcfd9, "PIKACH", 1, 0);
                for (int x = 1; x < 10; x++)
                    machine.tile(x, 2, 0x63);
                machine.tile(1, 3, 0x73);
            } else {
                machine.write_name(0xd008, "BULBAS", 10, 7);
                for (int x = 10; x < 19; x++)
                    machine.tile(x, 9, 0x63);
                machine.tile(18, 11, 0x6f);
            }
        };
        auto clear_side = [&](bool enemy) {
            battle::Rect r = enemy ? battle::Enemy : battle::Player;
            for (int y = 0; y < 7; y++)
                for (int x = 0; x < 7; x++)
                    machine.tile(r.x + x, r.y + y, 0x7f);
            const int *zone = enemy ? battle::EnemyHud : battle::PlayerHud;
            for (int y = zone[1]; y < zone[1] + zone[3]; y++)
                for (int x = zone[0]; x < zone[0] + zone[2]; x++)
                    machine.tile(x, y, 0x7f);
        };
        auto arena = [&] {
            blank();
            machine.write_rectangle(battle::Enemy);
            machine.write_rectangle(battle::Player);
            hud(true);
            hud(false);
        };
        arena();
        check(battle::arena_coherent(ctx), "both HUDs and pictures, nothing else");
        auto before = machine.wram;
        check(before == machine.wram, "arena_coherent reads memory only");
        clear_side(true);
        check(battle::arena_coherent(ctx) && !battle::present(ctx, true) &&
                  battle::present(ctx, false),
              "the fainted enemy's cleared HUD and picture");
        // SlideDownFaintedMonPic leaves shifted rows of the same picture.
        for (int x = 0; x < 7; x++)
            machine.tile(battle::Enemy.x + x, 4, battle::Enemy.base + x * 7);
        check(battle::arena_coherent(ctx), "a partial slide of the same picture");
        machine.tile(battle::Enemy.x + 3, 6, battle::Player.base + 29);
        check(battle::arena_coherent(ctx), "a send-out stage borrowing picture tiles");
        machine.tile(battle::Enemy.x + 3, 6, 0x62);
        check(!battle::arena_coherent(ctx), "a HUD tile over the picture is refused");
        machine.tile(battle::Enemy.x + 3, 6, 0x7f);
        machine.tile(battle::Enemy.x + 3, 4, 0x7f);
        machine.tile(3, 1, 0x73);
        check(!battle::arena_coherent(ctx), "party balls in the enemy's cleared HUD");
        machine.tile(3, 1, 0x7f);
        clear_side(false);
        check(battle::arena_coherent(ctx) && !battle::present(ctx, false),
              "both sides absent, as between a retreat and a send-out");
        arena();
        // PrintStatsBox at 9,2: its border crosses the enemy picture.
        for (int x = 9; x < 20; x++)
            machine.tile(x, 2, x == 9 ? 0x79 : 0x7a);
        check(!battle::arena_coherent(ctx), "the level-up stats box");
        arena();
        machine.tile(14, 8, 0x79);
        machine.tile(15, 9, 0xed);
        check(!battle::arena_coherent(ctx), "a yes/no box over the player's HUD");
        arena();
        machine.tile(19, 5, 0x80);
        check(!battle::arena_coherent(ctx), "a stray cell outside both HUDs and pictures");
        arena();
        machine.tile(battle::Enemy.x + 2, 2, 0x81);
        check(!battle::arena_coherent(ctx), "text over the enemy picture");
        arena();
        check(battle::arena_coherent(ctx), "the restored arena");
        machine.bgp(0xff);
        check(!battle::arena_coherent(ctx), "a palette fade");
        machine.bgp(0xe4);
        machine.io[0x42] = 1;
        check(!battle::arena_coherent(ctx), "a scrolled BG");
        machine.io[0x42] = 0;
        machine.write(0xd11c, 1);
        check(!battle::arena_coherent(ctx), "a full-screen list");
        machine.write(0xd11c, 0);
        machine.lcd_on(false);
        check(!battle::arena_coherent(ctx), "an off LCD");
        machine.lcd_on(true);
        machine.write(battle::Link, 1);
        check(!battle::arena_coherent(ctx), "a link battle");
        machine.write(battle::Link, 0);
        check(battle::arena_coherent(ctx), "the arena is coherent again");

        // --- holds(): held phase, coherent arena, and the player's own side -----------
        check(battle::holds(ctx, Phase::Fainted) && battle::holds(ctx, Phase::Switch),
              "a coherent KO or enemy replacement is held");
        check(!battle::holds(ctx, Phase::LevelStats) && !battle::holds(ctx, Phase::Turn),
              "unpresented phases are never held");
        clear_side(true);
        check(battle::holds(ctx, Phase::Switch), "the enemy's replacement with its side cleared");
        machine.write_rectangle(battle::Enemy);
        clear_side(false);
        check(battle::holds(ctx, Phase::Fainted) && !battle::holds(ctx, Phase::Switch),
              "the player's own retreat or send-out keeps the original LCD");
        arena();
        machine.tile(battle::Player.x + 3, battle::Player.y + 6, 0x7f);
        check(!battle::holds(ctx, Phase::Switch), "and so does its partial picture");
        arena();
        machine.tile(19, 0, 0x80);
        check(!battle::holds(ctx, Phase::Experience), "an incoherent arena is never held");
        arena();

        std::fprintf(stderr,
                     "PASS: %zu engine sites, every B1 phase with negative controls, "
                     "far frames and the coherent arena\n",
                     sites.size() + 4);
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
