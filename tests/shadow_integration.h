#pragma once
#include "imgui.h"

namespace shadow_qa {
inline std::vector<uint8_t> render(GBContext *ctx) {
    std::array<uint8_t, sizeof(GBContext)> registers{};
    std::memcpy(registers.data(), ctx, registers.size());
    const std::vector<uint8_t> rom(ctx->rom, ctx->rom + ctx->rom_size);
    auto image = presentation_qa::render(ctx);
    presentation_qa::require(!std::memcmp(registers.data(), ctx, registers.size()) &&
                                 !std::memcmp(rom.data(), ctx->rom, rom.size()) &&
                                 glGetError() == GL_NO_ERROR,
                             "shadow presentation preserves all context bytes and ROM");
    return image;
}
inline std::vector<uint8_t> depth_image(QaWalk &run) {
    GLint previous = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    GLuint target = 0;
    glGenFramebuffers(1, &target);
    glBindFramebuffer(GL_FRAMEBUFFER, target);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           pallet3d_shadows().texture, 0);
    run.require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                "actual shadow target readable after load");
    std::vector<uint8_t> bytes(1024 * 1024 * 4);
    glReadPixels(0, 0, 1024, 1024, GL_RGBA, GL_UNSIGNED_BYTE, bytes.data());
    glBindFramebuffer(GL_FRAMEBUFFER, GLuint(previous));
    glDeleteFramebuffers(1, &target);
    run.require(glGetError() == GL_NO_ERROR, "shadow load readback leaves no GL error");
    return bytes;
}
inline void loads(QaWalk &run, bool fp) {
    // Save during actual guest-driven wind; the existing world-effects policy
    // resets wind on load. Both a warm reload and a cold renderer must follow
    // that same policy, then replay identical engine frames and depth bytes.
    for (int i = 0; i < 24; ++i)
        run.tick();
    run.require(pallet3d_world_animation().wind > .1f, "save during active guest animation");
    const auto saved = world_animation_qa::snapshot(run, "logs/shadow-mid.state");
    std::vector<std::vector<uint8_t>> images, depths, states;
    for (int replay = 0; replay < 3; ++replay) {
        const bool artistic = replay != 2;
        if (replay == 1)
            pallet3d_shutdown();
        run.require(gb_context_load_state_file(run.ctx, "logs/shadow-mid.state"),
                    "load the same original mid-animation state");
        pallet3d_state_loaded(run.ctx);
        run.require(pallet3d_artistic(artistic) && pallet3d_daylight({daynight::Mode::Fixed, 12}),
                    "loaded shadow presentation settings");
        if (pallet3d_firstperson() != fp)
            special_transition_qa::key(run.ctx, SDL_SCANCODE_F3);
        for (int i = 0; i < 90; ++i)
            render(run.ctx);
        run.require(pallet3d_world_animation().wind == 0,
                    "cold and warm load clear wind without advancing guest time");
        run.require(world_animation_qa::snapshot(run, "logs/shadow-load.state") == saved,
                    "loading and settling preserve every serialized engine byte");
        uint32_t previous_cycles = run.ctx->cycles;
        uint64_t elapsed_cycles = 0;
        const auto initial_passes = pallet3d_shadows().passes;
        size_t changed_depths = 0;
        for (int frame = 0; frame <= 24; ++frame) {
            if (frame) {
                gb_reset_frame(run.ctx);
                run.ctx->stopped = 0;
                unsigned slices = 0;
                while (!run.ctx->frame_done) {
                    gb_run_cycles(run.ctx, 70224);
                    run.require(gb_platform_poll_events(run.ctx) && ++slices < 1000,
                                "original guest completes shadow replay frame");
                }
            }
            auto image = render(run.ctx);
            const uint32_t now = run.ctx->cycles;
            elapsed_cycles += uint32_t(now - previous_cycles);
            previous_cycles = now;
            run.require(std::abs(pallet3d_world_animation().wind -
                                 float(double(elapsed_cycles) / 4194304.0)) < .000001f,
                        "shadow wind follows elapsed original CPU cycles after load");
            auto state = world_animation_qa::snapshot(run, "logs/shadow-replay.state");
            run.require(pallet3d_shadows().active == artistic,
                        "only enabled loaded scene submits shadows");
            if (replay == 0) {
                images.push_back(std::move(image));
                depths.push_back(depth_image(run));
                states.push_back(std::move(state));
                changed_depths += frame && depths.back() != depths.front();
            } else {
                run.require(state == states[size_t(frame)],
                            "every replay engine byte matches warm, cold and artistic OFF");
                if (artistic)
                    run.require(image == images[size_t(frame)] &&
                                    depth_image(run) == depths[size_t(frame)],
                                "cold and warm loads reproduce exact image and shadow depth");
                else
                    run.require(pallet3d_shadows().passes == initial_passes,
                                "disabled replay never submits a shadow pass");
            }
        }
        if (artistic)
            run.require(pallet3d_shadows().passes == initial_passes + 24,
                        "each advancing guest wind phase refreshes the actual depth target");
        if (replay == 0)
            std::printf("[SHADOW-LOAD] changed depth images=%zu/24 (camera may see no moving "
                        "casters); actual refreshed targets=24\n",
                        changed_depths);
    }
    std::puts("PASS: shadow mid-animation and cold loads; 25 exact frames/depths, "
              "75 complete engine states and independent CPU-cycle wind oracle");
}
inline void hero_alpha(QaWalk &run, const npc_animation::Image &expected) {
    GLint previous = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    GLuint target = 0;
    glGenFramebuffers(1, &target);
    glBindFramebuffer(GL_FRAMEBUFFER, target);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           pallet3d_tile_animation().texture, 0);
    run.require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                "first-person hero atlas readable");
    std::array<uint8_t, 32 * 32 * 4> rgba{};
    glReadPixels(0, 384, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    size_t opaque = 0;
    for (size_t i = 0; i < expected.size(); ++i) {
        run.require(rgba[i * 4 + 3] == (expected[i] ? 255 : 0),
                    "invisible hero shadow alpha matches independent original VRAM oracle");
        opaque += expected[i] != 0;
    }
    run.require(opaque > 0 && opaque < expected.size(),
                "hero has a silhouette and transparent padding");
    glBindFramebuffer(GL_FRAMEBUFFER, GLuint(previous));
    glDeleteFramebuffers(1, &target);
    run.require(glGetError() == GL_NO_ERROR, "hero alpha readback leaves no GL error");
}
inline int captures(GBContext *ctx, bool fp) {
    QaWalk run{ctx};
    run.input(nullptr);
    run.wait(60);
    run.require(pallet::view(ctx) == pallet::View::Overworld, "live exterior shadow fixture");
    if (fp)
        special_transition_qa::key(ctx, SDL_SCANCODE_F3);
    for (int i = 0; i < 90; ++i)
        render(ctx);
    run.require(pallet3d_firstperson() == fp, "requested shadow camera actually active");
    const auto state = world_animation_qa::snapshot(run, "logs/shadow-before.state");
    size_t changed_hours = 0;
    for (double hour : {-1., 0., 6., 6.5, 12., 17.5, 18., 21.}) {
        auto light =
            hour < 0 ? daynight::Settings{} : daynight::Settings{daynight::Mode::Fixed, hour};
        run.require(pallet3d_daylight(light) && pallet3d_artistic(false), "reference presentation");
        auto reference = render(ctx);
        std::string label = "logs/hour-" + std::to_string(hour);
        capture_surface((label + "-off.ppm").c_str());
        const auto old_passes = pallet3d_shadows().passes;
        run.require(pallet3d_artistic(true), "enable artistic shadow presentation");
        auto image = render(ctx);
        capture_surface((label + "-on.ppm").c_str());
        auto info = pallet3d_shadows();
        const bool sunlight = hour > 6 && hour < 18;
        run.require(info.active == sunlight && info.artistic_scene && info.ready,
                    "actual shadow submission follows sun visibility");
        if (sunlight) {
            run.require(info.passes > old_passes, "lit scene submits depth after geometry change");
            changed_hours += image != reference;
            if (fp) {
                run.require(info.player_caster && !pallet3d_camera().player_drawn,
                            "first-person hero casts while remaining invisible");
                pallet::Actor hero;
                run.require(pallet::actor(ctx, 0, hero), "original engine supplies the hero");
                hero_alpha(run, world_animation_qa::vram_image(ctx, hero.image));
            }
        } else {
            run.require(info.passes == old_passes && image == reference,
                        "no sun: zero shadow passes and byte-exact reference image");
        }
        for (int repeat = 0; repeat < 3; ++repeat) {
            run.require(render(ctx) == image, "frozen shadow image is deterministic");
            run.require(pallet3d_shadows().passes == info.passes,
                        "unchanged presentation reuses the actual depth target");
        }
        run.require(world_animation_qa::snapshot(run, "logs/shadow-after.state") == state,
                    "shadow passes preserve the complete engine snapshot");
        run.require(pallet3d_artistic(false), "restore reference presentation");
        run.require(render(ctx) == reference, "toggle restores every reference byte for this hour");
    }
    run.require(changed_hours > 0, "sunlight must create visible shadows in the real scene");
    run.require(pallet3d_artistic(true) && pallet3d_daylight({daynight::Mode::Fixed, 12}),
                "lit pause fixture");
    auto stable = render(ctx);
    const auto passes = pallet3d_shadows().passes;
    SDL_Event event{};
    event.type = SDL_WINDOWEVENT;
    event.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    pallet3d_event(&event, false);
    for (int i = 0; i < 3; ++i) {
        SDL_Delay(40);
        run.require(render(ctx) == stable && pallet3d_shadows().passes == passes,
                    "focus loss preserves the image and cached depth");
    }
    event.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
    pallet3d_event(&event, false);
    run.require(render(ctx) == stable, "focus return preserves shadow phase");
    special_transition_qa::key(ctx, SDL_SCANCODE_ESCAPE);
    for (int i = 0; i < 3; ++i) {
        render(ctx);
        run.require(ImGui::GetForegroundDrawList()->VtxBuffer.empty(),
                    "no artistic foreground above the native settings panel");
        run.require(pallet3d_shadows().passes == passes, "Esc freezes shadow state");
    }
    capture_surface("logs/settings-shadow.ppm");
    special_transition_qa::key(ctx, SDL_SCANCODE_ESCAPE);
    run.require(render(ctx) == stable, "closing Esc restores identical lit image");
    run.require(world_animation_qa::snapshot(run, "logs/shadow-after.state") == state,
                "focus and settings preserve every guest-state byte");
    loads(run, fp);
    std::printf("PASS: B1 shadows, eight sun states, %zu visible changes, camera=%s; "
                "cache, alpha hero, focus, Esc and full engine invariants\n",
                changed_hours, fp ? "fp" : "ortho");
    return 0;
}
} // namespace shadow_qa
