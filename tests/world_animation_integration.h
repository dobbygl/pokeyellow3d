#pragma once
#include "npc_animation.h"
#include <fstream>
#include <iterator>
#include <set>

namespace world_animation_qa {
// Independent oracle follows the resident OAM tile indices. It never calls
// the ROM sheet decoder, so a wrong sheet/stride/walking half is detectable.
inline npc_animation::Image vram_image(GBContext *ctx, int image) {
    npc_animation::Image out{};
    int frame = image < 0xa0 ? image & 15 : 0;
    size_t ptr = 0x4000 + size_t(frame) * 2;
    size_t at = size_t(ctx->rom[ptr]) | (size_t(ctx->rom[ptr + 1]) << 8);
    int count = ctx->rom[at++];
    int base = image >> 4;
    base = base == 11 ? 124 : base * 12;
    for (int i = 0; i < count; ++i, at += 4) {
        int dy = int8_t(ctx->rom[at]), dx = int8_t(ctx->rom[at + 1]);
        int tile = (base + ctx->rom[at + 2]) & 255, flags = ctx->rom[at + 3];
        if (tile >= 128)
            tile = (tile + ctx->hram[0x7c]) & 255;
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                int sx = flags & 0x20 ? 7 - x : x, sy = flags & 0x40 ? 7 - y : y;
                int px = 8 + dx + x, py = 8 + dy + y;
                if (px >= 0 && px < 32 && py >= 0 && py < 32)
                    out[size_t(py) * 32 + px] =
                        uint8_t(((ctx->vram[tile * 16 + sy * 2] >> (7 - sx)) & 1) |
                                (((ctx->vram[tile * 16 + sy * 2 + 1] >> (7 - sx)) & 1) << 1));
            }
    }
    return out;
}
inline void verify_gpu(QaWalk &run, int slot, const npc_animation::Image &expected) {
    GLint previous = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           pallet3d_tile_animation().texture, 0);
    run.require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                "NPC private GPU atlas readable");
    std::array<uint8_t, 32 * 32 * 4> rgba{};
    glReadPixels(slot * 32, 384, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    constexpr float palette[4][4]{
        {0, 0, 0, 0}, {.97f, .91f, .73f, 1}, {.31f, .51f, .57f, 1}, {.16f, .20f, .22f, 1}};
    for (size_t i = 0; i < expected.size(); ++i)
        for (size_t c = 0; c < 4; ++c)
            run.require(rgba[i * 4 + c] == uint8_t(palette[expected[i]][c] * 255),
                        "uploaded distant NPC pixels equal original ROM sheet");
    glBindFramebuffer(GL_FRAMEBUFFER, GLuint(previous));
    glDeleteFramebuffers(1, &fbo);
}
inline int npc(GBContext *ctx, bool fp) {
    QaWalk run{ctx};
    run.input(nullptr);
    run.wait(30);
    if (run.read(pallet::Map) == 0 && run.read(pallet::X) == 9 && run.read(pallet::Y) == 7)
        run.move(0, 7, 7, "L");
    if (fp)
        special_transition_qa::key(ctx, SDL_SCANCODE_F3);
    int resident = 0, distant = 0, moving = 0;
    std::set<int> frames, pictures;
    FILE *trace = std::fopen("logs/npc-animation.csv", "w");
    run.require(trace, "NPC animation trace");
    std::fputs("frame,slot,picture,image,status,step,animation,outside,x,z\n", trace);
    for (int frame = 0; frame < 600; ++frame) {
        run.tick();
        for (int slot = 1; slot < 15; ++slot) {
            pallet::Actor actor;
            if (!pallet::actor(ctx, slot, actor))
                continue;
            const auto *a = ctx->wram + 0x100 + slot * 16;
            const auto *b = ctx->wram + 0x200 + slot * 16;
            int image = a[2];
            auto motion = npc_animation::sample(ctx, slot);
            int selected = image == 255 ? motion.frame : image & 15;
            npc_animation::Image decoded;
            run.require(npc_animation::decode(ctx, slot, selected, decoded),
                        "live NPC sheet recognized in ROM");
            bool outside = image == 255 || a[6] >= 160 || a[4] >= 144;
            pictures.insert(a[0]);
            if (image != 255) {
                if (decoded != vram_image(ctx, image))
                    std::fprintf(stderr, "[NPC] mismatch picture=%d image=%02x slot=%d\n", a[0],
                                 image, slot);
                run.require(decoded == vram_image(ctx, image),
                            "ROM sheet matches original resident VRAM frame");
                ++resident;
            }
            if (outside) {
                ++distant;
                if (frame % 20 == 0)
                    verify_gpu(run, slot, decoded);
            }
            if (motion.walking) {
                ++moving;
                frames.insert(motion.frame & 3);
            }
            std::fprintf(trace, "%d,%d,%d,%d,%d,%d,%d,%d,%.4f,%.4f\n", frame, slot, a[0], image,
                         a[1], b[0], a[8], int(outside), actor.x, actor.z);
        }
        if (frame % 60 == 0)
            capture_surface(("logs/npc-" + std::to_string(frame) + ".ppm").c_str());
    }
    std::fclose(trace);
    run.require(resident > 0 && distant > 0, "resident and distant NPCs audited");
    run.require(moving > 0 && frames.size() == 4, "original engine supplies every walking frame");
    // A private fixture requests the engine's ordinary scripted NPC movement.
    // Unlike random NPCs, this path can advance outside the LCD visibility box.
    // $cc5b is verified by the matching ROM's LD DE at 01:4ea2 (an aliased buffer
    // in pokeyellow_internal.h). Only setup writes memory; all subsequent steps,
    // counters, visibility and frame selection come from the original engine.
    run.require(run.read(pallet::Map) == 0, "Pallet scripted NPC fixture");
    auto *a = ctx->wram + 0x120, *b = ctx->wram + 0x220;
    int steps = int(b[5]) - 4 - 1;
    run.require(steps > 0 && steps < 10, "script remains inside Pallet bounds");
    a[1] = 1;
    a[7] = a[8] = 0;
    b[0] = b[6] = 0;
    ctx->wram[0xf0f] = uint8_t(steps);
    for (int i = 0; i <= steps; ++i)
        ctx->wram[0xc5b + i] = uint8_t(i < steps ? 0x80 : 0xff);
    std::set<int> outside_frames;
    std::set<float> outside_x;
    FILE *script = std::fopen("logs/npc-script.csv", "w");
    run.require(script, "scripted NPC trace");
    std::fputs("frame,image,status,remaining,phase,x,z\n", script);
    for (int frame = 0; frame < 220; ++frame) {
        run.tick();
        pallet::Actor actor;
        if (!pallet::actor(ctx, 2, actor))
            continue;
        auto motion = npc_animation::sample(ctx, 2);
        bool outside = a[2] == 255 || a[6] >= 160 || a[4] >= 144;
        std::fprintf(script, "%d,%u,%u,%u,%u,%.4f,%.4f\n", frame, a[2], a[1], b[0], a[8], actor.x,
                     actor.z);
        if (outside && motion.walking) {
            outside_frames.insert(motion.frame & 3);
            outside_x.insert(actor.x);
            npc_animation::Image decoded;
            int selected = a[2] == 255 ? motion.frame : a[2] & 15;
            run.require(npc_animation::decode(ctx, 2, selected, decoded), "scripted ROM sheet");
            verify_gpu(run, 2, decoded);
            if (frame % 4 == 0)
                capture_surface(("logs/npc-outside-" + std::to_string(frame) + ".ppm").c_str());
        }
    }
    std::fclose(script);
    std::fprintf(stderr, "[NPC] scripted outside phases=%zu positions=%zu\n", outside_frames.size(),
                 outside_x.size());
    run.require(outside_frames.size() == 4 && outside_x.size() > 8,
                "NPC advances through all walking frames outside original LCD");
    std::printf("PASS: NPC resident=%d distant=%d moving=%d frames=%zu pictures=%zu; ROM/VRAM/GPU "
                "and per-frame guards\n",
                resident, distant, moving, frames.size(), pictures.size());
    return 0;
}
inline std::vector<uint8_t> snapshot(QaWalk &run, const char *path) {
    run.require(gb_context_save_state_file(run.ctx, path),
                "serialize complete private engine state");
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
inline int effects(GBContext *ctx, bool fp, bool surf) {
    QaWalk run{ctx};
    run.input(nullptr);
    run.wait(30);
    if (fp)
        special_transition_qa::key(ctx, SDL_SCANCODE_F3);
    run.require((surf && run.read(0xd6ff) == 2) || (!surf && run.read(pallet::Map) == 12),
                "active surf or Route 1 grass fixture");
    run.require(gb_context_save_state_file(ctx, "logs/effects-start.state"),
                "effects replay start");
    std::vector<std::vector<uint8_t>> states;
    size_t maximum = 0;
    uint32_t emitted = 0;
    int crossings = 0;
    FILE *trace = std::fopen("logs/effects.csv", "w");
    run.require(trace, "effects trace");
    std::fputs("enabled,frame,map,x,y,battle,wind,particles,emitted\n", trace);
    for (bool enabled : {true, false}) {
        run.require(gb_context_load_state_file(ctx, "logs/effects-start.state"),
                    "identical replay start");
        pallet3d_world_effects(enabled);
        pallet3d_state_loaded(ctx);
        run.input(nullptr);
        gb_platform_render_frame(gb_get_framebuffer(ctx));
        auto fresh = pallet3d_world_animation();
        run.require(fresh.wind == 0 && fresh.particles == 0 && fresh.emitted == 0,
                    "load clears effects before advancing engine");
        for (int frame = 0; frame < 300; ++frame) {
            if (frame % 150 == 0)
                run.input((frame / 150) % 2 ? "U" : "D");
            if (frame % 150 == 120)
                run.input(nullptr);
            int previous_map = run.read(pallet::Map);
            run.tick();
            auto info = pallet3d_world_animation();
            if (enabled) {
                maximum = std::max(maximum, info.particles);
                emitted = std::max(emitted, info.emitted);
                if (previous_map != run.read(pallet::Map)) {
                    ++crossings;
                    run.require(info.particles == 0 && info.emitted == 0,
                                "actual map crossing resets the particle pool");
                }
            } else
                run.require(info.wind == 0 && info.particles == 0,
                            "disabled effects leave no wind or trail");
            auto state = snapshot(run, "logs/replay-current.state");
            run.require(!state.empty(), "complete serialized snapshot exists");
            if (enabled)
                states.push_back(std::move(state));
            else
                run.require(state == states[size_t(frame)],
                            "every byte of every engine snapshot matches with effects on/off");
            std::fprintf(trace, "%d,%d,%d,%d,%d,%d,%.8f,%zu,%u\n", int(enabled), frame,
                         run.read(pallet::Map), run.read(pallet::X), run.read(pallet::Y),
                         run.read(pallet::Battle), info.wind, info.particles, info.emitted);
            if (enabled && frame < 180 && frame % 6 == 0)
                capture_surface(("logs/effects-" + std::to_string(frame) + ".ppm").c_str());
            if (enabled && frame == 12) {
                ReadOnlyMemory before{ctx};
                for (int i = 0; i < 12; ++i)
                    gb_platform_render_frame(gb_get_framebuffer(ctx));
                auto frozen = pallet3d_world_animation();
                run.require(before.unchanged(ctx) && frozen.wind == info.wind &&
                                frozen.particles == info.particles &&
                                frozen.emitted == info.emitted,
                            "frozen engine freezes wind and particles without memory writes");
                special_transition_qa::key(ctx, SDL_SCANCODE_ESCAPE);
                for (int i = 0; i < 12; ++i)
                    gb_platform_render_frame(gb_get_framebuffer(ctx));
                auto paused = pallet3d_world_animation();
                run.require(before.unchanged(ctx) && paused.wind == info.wind &&
                                paused.particles == info.particles &&
                                paused.emitted == info.emitted,
                            "settings pause freezes presentation effects");
                special_transition_qa::key(ctx, SDL_SCANCODE_ESCAPE);
            }
        }
        snapshot(run, enabled ? "logs/effects-on.state" : "logs/effects-off.state");
    }
    std::fclose(trace);
    run.require(maximum > 0 && maximum <= 96 && emitted >= 3,
                "observed movement creates bounded grass/surf particles");
    run.require(!surf || crossings >= 2,
                "surf crosses and returns through the original map connection");
    if (!surf) {
        run.require(gb_context_load_state_file(ctx, "logs/effects-start.state"),
                    "stationary wind sequence starts from the same private state");
        pallet3d_world_effects(true);
        pallet3d_state_loaded(ctx);
        run.input(nullptr);
        const int x = run.read(pallet::X), y = run.read(pallet::Y);
        gb_platform_render_frame(gb_get_framebuffer(ctx));
        capture_surface("logs/wind-0.ppm");
        for (int frame = 1; frame <= 120; ++frame) {
            run.tick();
            auto info = pallet3d_world_animation();
            run.require(run.read(pallet::X) == x && run.read(pallet::Y) == y &&
                            info.particles == 0 && info.emitted == 0,
                        "stationary wind never creates movement or particles");
            if (frame % 30 == 0)
                capture_surface(("logs/wind-" + std::to_string(frame) + ".ppm").c_str());
        }
        run.require(pallet3d_world_animation().wind > 1, "stationary wind advances in guest time");
    }
    std::printf("PASS: %s particles maximum=%zu emitted=%u; 300 complete engine snapshots "
                "identical on/off, pause/load guards\n",
                surf ? "surf" : "grass", maximum, emitted);
    return 0;
}
} // namespace world_animation_qa
