#pragma once
// B1: plays a private battle savestate with a fixed input script and records
// the sequence of battle::Phase values, one entry per change. It checks the
// expected engine order, that the held phases (KO, experience, level-up and
// learned-move messages, the enemy's replacement) keep the arena in the
// integrated style, that the unpresented subphases keep the complete original
// LCD (negative controls) and that Classic never holds a phase. Memory stays
// read-only and GL error free on every frame.
#include "read_only_memory.h"
#include <string>
#include <vector>

namespace battle_phases_qa {
inline std::string script(bool trainer) {
    // A every 40 frames; the trainer run answers NO to "Will ASH change
    // POKéMON?" so the rival's next Pokemon is sent out.
    std::string out;
    auto press = [&](int frame, const char *button) {
        out += (out.empty() ? "" : ",") + std::to_string(frame) + ":" + button + ":4";
    };
    for (int f = 5; f < (trainer ? 2806 : 700); f += 40)
        press(f, "A");
    if (trainer) {
        press(2825, "D");
        for (int f = 2845; f < 3100; f += 40)
            press(f, "A");
    }
    return out;
}
inline int run(GBContext *ctx, bool trainer) {
    using battle::Phase;
    const bool styled = pallet3d_menu_style() == ui_preferences::Style::Integrated;
    gb_platform_set_input_script(script(trainer).c_str());
    const int frames = trainer ? 3100 : 720;
    std::vector<Phase> sequence;
    int held_frames = 0, fallback_frames = 0, negative_frames = 0, incoherent_frames = 0;
    for (int frame = 0; frame < frames; frame++) {
        gb_reset_frame(ctx);
        ctx->stopped = 0;
        unsigned slices = 0;
        while (!ctx->frame_done) {
            gb_run_cycles(ctx, 70224);
            if (!gb_platform_poll_events(ctx) || ++slices > 1000)
                return 12;
        }
        Phase phase = battle::phase(ctx);
        bool coherent = battle::arena_coherent(ctx);
        ReadOnlyMemory memory{ctx};
        gb_platform_render_frame(gb_get_framebuffer(ctx));
        if (!memory.unchanged(ctx) || glGetError() != GL_NO_ERROR) {
            std::fprintf(stderr, "FAIL: memory or GL changed at frame %d\n", frame);
            return 16;
        }
        auto info = pallet3d_battle();
        if (info.phase != int(phase)) {
            std::fprintf(stderr, "FAIL: presented phase %d differs from %s at frame %d\n",
                         info.phase, battle::phase_name(phase), frame);
            return 30;
        }
        if (sequence.empty() || sequence.back() != phase)
            sequence.push_back(phase);
        bool presented = phase == Phase::Fainted || phase == Phase::Experience ||
                         phase == Phase::LevelUp || phase == Phase::LearnMove;
        bool unpresented = phase == Phase::LevelStats || phase == Phase::PartyBalls ||
                           phase == Phase::SwitchYesNo || phase == Phase::SwitchParty;
        if (!styled && info.held) {
            std::fprintf(stderr, "FAIL: Classic held %s at frame %d\n", battle::phase_name(phase),
                         frame);
            return 31;
        }
        if (styled && presented && !info.held) {
            std::fprintf(stderr, "FAIL: %s not held at frame %d\n", battle::phase_name(phase),
                         frame);
            return 32;
        }
        if (unpresented && info.held) {
            std::fprintf(stderr, "FAIL: %s held at frame %d\n", battle::phase_name(phase), frame);
            return 33;
        }
        // Decision 8 on real VRAM: a held phase the tile map contradicts (the
        // enemy's party balls beside "is about to use") is never held.
        if (battle::held_phase(phase) && !coherent) {
            if (info.held) {
                std::fprintf(stderr, "FAIL: incoherent %s held at frame %d\n",
                             battle::phase_name(phase), frame);
                return 37;
            }
            incoherent_frames += info.full_overlay;
        }
        held_frames += info.held;
        fallback_frames += presented && info.full_overlay;
        negative_frames += unpresented && info.full_overlay;
    }
    std::string log;
    for (auto p : sequence)
        log += std::string(log.empty() ? "" : " ") + battle::phase_name(p);
    std::fprintf(stderr, "[PHASES] %s\n", log.c_str());
    std::vector<Phase> expected =
        trainer ? std::vector<Phase>{Phase::TrainerIntro, Phase::Turn,       Phase::Fainted,
                                     Phase::Experience,   Phase::LevelUp,    Phase::LevelStats,
                                     Phase::LearnMove,    Phase::PartyBalls, Phase::Switch,
                                     Phase::SwitchYesNo,  Phase::Switch,     Phase::Turn}
                : std::vector<Phase>{Phase::Turn, Phase::Fainted, Phase::Experience, Phase::None};
    size_t matched = 0;
    for (auto p : sequence)
        if (matched < expected.size() && p == expected[matched])
            ++matched;
    if (matched != expected.size()) {
        std::fprintf(stderr, "FAIL: expected phase %s in order\n",
                     battle::phase_name(expected[matched]));
        return 34;
    }
    if (styled ? !held_frames : fallback_frames == 0) {
        std::fprintf(stderr, "FAIL: %s\n",
                     styled ? "no held frame" : "Classic lost the original LCD fallback");
        return 35;
    }
    if (trainer && (!negative_frames || !incoherent_frames)) {
        std::fprintf(stderr, "FAIL: unpresented or incoherent frames never showed the LCD\n");
        return 36;
    }
    std::printf("PASS: %s %s phases in engine order, %d held frames, original LCD in %d "
                "unpresented and %d incoherent frames, memory and GL unchanged\n",
                trainer ? "trainer" : "wild", styled ? "integrated" : "classic", held_frames,
                negative_frames, incoherent_frames);
    return 0;
}
} // namespace battle_phases_qa
