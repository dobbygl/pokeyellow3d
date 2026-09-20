#pragma once
#include "tile_animation.h"
#include <set>

namespace tile_animation_qa {
inline int run(GBContext *ctx, bool fp) {
    QaWalk run{ctx};
    run.input(nullptr);
    run.wait(50);
    if (fp) {
        SDL_Event event{};
        event.type = SDL_KEYDOWN;
        event.key.keysym.scancode = SDL_SCANCODE_F3;
        pallet3d_event(&event, false);
    }
    const auto *scene = pallet::scene(run.read(pallet::Map));
    run.require(scene, "resident scene");
    const auto tiles = pallet::tileset(*scene);
    run.require(ctx->hram[tile_animation::Flag] == tiles.animations,
                "ROM tileset flag agrees with live HRAM");
    run.require(gb_context_save_state_file(ctx, "logs/animation-start.state"),
                "private phase save");
    auto water_bytes = [&] {
        tile_animation::Tile result;
        std::copy_n(ctx->vram + 0x1140, 16, result.begin());
        return result;
    };
    auto flower_bytes = [&] {
        tile_animation::Tile result;
        std::copy_n(ctx->vram + 0x1030, 16, result.begin());
        return result;
    };
    // Read back the actual private GPU atlas, not a duplicate CPU decoder.
    auto verify_atlas = [&] {
        const auto *displayed = pallet::scene(run.read(pallet::Map));
        const auto &displayed_tiles = pallet::tileset(*displayed);
        auto info = pallet3d_tile_animation();
        run.require(info.texture != 0, "private GPU atlas exists");
        GLint old = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old);
        GLuint fbo = 0;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, info.texture,
                               0);
        run.require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                    "atlas readback framebuffer");
        int slot = displayed->interior        ? 0
                   : displayed_tiles.id == 3  ? 1
                   : displayed_tiles.id == 14 ? 2
                   : displayed_tiles.id == 23 ? 3
                                              : 0;
        for (int tile : {tile_animation::Water, tile_animation::Flower}) {
            // Palette two is the same four water colours in every tileset.
            std::array<uint8_t, 256> pixels{};
            glReadPixels(slot * 128 + (tile % 16) * 8, 256 + (tile / 16) * 8, 8, 8, GL_RGBA,
                         GL_UNSIGNED_BYTE, pixels.data());
            auto bytes = tile_animation::frame(ctx, displayed_tiles, tile,
                                               {info.water_shift, info.flower_frame});
            constexpr float palette[4][3] = {
                {.64f, .87f, .86f}, {.35f, .67f, .71f}, {.19f, .47f, .57f}, {.13f, .34f, .43f}};
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    int colour = ((bytes[y * 2] >> (7 - x)) & 1) |
                                 (((bytes[y * 2 + 1] >> (7 - x)) & 1) << 1);
                    for (int c = 0; c < 3; ++c)
                        run.require(pixels[(y * 8 + x) * 4 + c] ==
                                        uint8_t(palette[colour][c] * 255),
                                    "GPU texel matches ROM animation");
                }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, GLuint(old));
        glDeleteFramebuffers(1, &fbo);
        run.require(glGetError() == GL_NO_ERROR, "GPU animation readback");
    };
    FILE *trace = std::fopen("logs/animation.csv", "w");
    run.require(trace, "animation trace");
    std::fputs("frame,flag,counter,step,water,flower,uploads\n", trace);
    std::set<int> water_phases, flower_phases, steps;
    int water_ticks = 0, flower_ticks = 0;
    // More than two complete eight-step periods (8*21 or 8*20 VBlanks).
    for (int frame = 0; frame < 360; ++frame) {
        auto old_water = water_bytes(), old_flower = flower_bytes();
        int counter = ctx->hram[tile_animation::Counter], step = run.read(tile_animation::Step);
        run.tick();
        int next_counter = ctx->hram[tile_animation::Counter],
            next_step = run.read(tile_animation::Step);
        const auto phase = tile_animation::sample(ctx, tiles);
        auto info = pallet3d_tile_animation();
        if (tiles.animations) {
            run.require(phase.water >= 0 && phase.water == info.water_shift,
                        "every water frame recognized and presented");
            run.require(tile_animation::frame(ctx, tiles, tile_animation::Water, phase) ==
                            water_bytes(),
                        "ROM reconstruction equals live VRAM water");
            steps.insert(next_step);
            if (next_step != step) {
                ++water_ticks;
                run.require(next_step == ((step + 1) & 7), "original eight-step water cycle");
                run.require(counter == 19 && next_counter == (tiles.animations == 1 ? 0 : 20),
                            "original water cadence at VBlank twenty");
                auto expected = old_water;
                for (auto &byte : expected)
                    byte = next_step & 4 ? uint8_t(byte * 2 + byte / 128)
                                         : uint8_t(byte / 2 + (byte & 1) * 128);
                run.require(expected == water_bytes(), "original alternating rotate direction");
            } else
                run.require(old_water == water_bytes(), "water stays still between engine updates");
            water_phases.insert(phase.water);
        } else {
            run.require(phase.water < 0 && phase.flower < 0, "disabled tileset remains static");
            run.require(water_bytes() == old_water && flower_bytes() == old_flower,
                        "non-animated VRAM remains unchanged");
        }
        if (tiles.animations == 2) {
            run.require(phase.flower >= 0 && phase.flower == info.flower_frame,
                        "every flower frame recognized and presented");
            run.require(tile_animation::frame(ctx, tiles, tile_animation::Flower, phase) ==
                            flower_bytes(),
                        "ROM reconstruction equals live VRAM flower");
            if (counter == 20 && next_counter == 0) {
                ++flower_ticks;
                int index = next_step & 3;
                run.require(phase.flower == (index < 2 ? 0 : index - 1),
                            "flower copies on VBlank twenty-one with original sequence");
            } else
                run.require(old_flower == flower_bytes(), "flower stable between copy ticks");
            flower_phases.insert(phase.flower);
        }
        if (frame % 20 == 0)
            verify_atlas();
        if (frame < 170 && (next_step != step || counter == 20)) {
            std::string path = "logs/frame-" + std::to_string(frame) + ".ppm";
            capture_surface(path.c_str());
        }
        std::fprintf(trace, "%d,%d,%d,%d,%d,%d,%zu\n", frame, ctx->hram[tile_animation::Flag],
                     next_counter, next_step, phase.water, phase.flower, info.uploads);
    }
    std::fclose(trace);
    run.require(!tiles.animations ||
                    (steps.size() == 8 && water_ticks >= 16 && water_phases.size() > 1),
                "two full water animation periods");
    run.require(tiles.animations != 2 || (flower_ticks >= 16 && flower_phases.size() == 3),
                "two full flower animation periods");
    ReadOnlyMemory paused{ctx};
    auto uploads = pallet3d_tile_animation().uploads;
    for (int i = 0; i < 12; ++i)
        gb_platform_render_frame(gb_get_framebuffer(ctx));
    run.require(paused.unchanged(ctx) && uploads == pallet3d_tile_animation().uploads,
                "paused guest neither advances animation nor uploads unchanged tiles");
    run.require(gb_context_load_state_file(ctx, "logs/animation-start.state"),
                "reload private phase");
    pallet3d_state_loaded(ctx);
    ReadOnlyMemory loaded{ctx};
    gb_platform_render_frame(gb_get_framebuffer(ctx));
    run.require(loaded.unchanged(ctx), "reload presentation is read-only");
    verify_atlas();
    auto phase = tile_animation::sample(ctx, tiles);
    run.require(pallet3d_tile_animation().water_shift == phase.water &&
                    pallet3d_tile_animation().flower_frame == phase.flower,
                "reload restores resident phase immediately");
    if (scene->id == 0) {
        // Retained exterior atlas must return to the ROM reference phase in
        // catalog previews, then immediately recover the live phase.
        pallet3d_preview(0);
        gb_platform_render_frame(gb_get_framebuffer(ctx));
        run.require(pallet3d_tile_animation().water_shift == -1 &&
                        pallet3d_tile_animation().flower_frame == -1,
                    "fixed catalog reference phase");
        verify_atlas();
        pallet3d_preview(-1);
        gb_platform_render_frame(gb_get_framebuffer(ctx));
        verify_atlas();
        run.move(0, 9, 8, "D");
        run.move(0, 5, 8, "L");
        run.move(37, 2, 7, "U");
        run.input(nullptr);
        run.wait(50);
        run.require(pallet3d_tile_animation().water_shift == -1 &&
                        pallet3d_tile_animation().flower_frame == -1,
                    "real house crossing disables both animated tiles");
        verify_atlas();
        run.move(0, 5, 6, "D");
        run.input(nullptr);
        run.wait(50);
        verify_atlas();
        run.require(pallet3d_tile_animation().water_shift >= 0 &&
                        pallet3d_tile_animation().flower_frame >= 0,
                    "original exit restores live exterior animation");
    }
    std::printf("PASS: map=%d flags=%d water_ticks=%d flower_ticks=%d phases=%zu/%zu; GPU, VRAM, "
                "pause, reload and memory guards\n",
                scene->id, tiles.animations, water_ticks, flower_ticks, water_phases.size(),
                flower_phases.size());
    return 0;
}
} // namespace tile_animation_qa
