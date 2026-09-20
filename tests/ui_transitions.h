#pragma once
#include <set>

// Observe the real engine's frames, including the gap between LoadMapData's
// return and LoadGBPal. A successful endpoint alone cannot detect a 2D flash.
namespace transition_qa {
inline int settled = -1, destination = -1, segments = 0, frames = 0, last_bgp = -1;
inline PalletWorldFrameInfo frozen{};
inline Pallet3DStats resident{};
inline FILE *video = nullptr;
inline std::vector<uint8_t> pixels, reference;
inline fade::Tone reference_tone{};
inline std::set<int> palettes;
inline void require(bool ok, const char *message, int frame) {
    if (!ok) {
        std::fprintf(stderr, "[TRANSITION] FAIL frame=%d: %s\n", frame, message);
        std::exit(41);
    }
}
inline void observe(GBContext *ctx, int frame) {
    auto state = pallet::view(ctx);
    int map = pallet::read(ctx, pallet::Map), bgp = ctx->io[0x47];
    bool warp = pallet3d_warp_overlay();
    if (settled < 0 && state == pallet::View::Overworld && !warp)
        settled = map;
    if (destination < 0 && settled >= 0 && (warp || map != settled)) {
        destination = map;
        ++segments;
        last_bgp = -1;
        reference.clear();
        frozen = pallet3d_world_frame();
        resident = pallet3d_stats();
        std::fprintf(stderr, "[TRANSITION] begin %d -> %d frame=%d\n", settled, map, frame);
    }
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int w = viewport[2], h = viewport[3];
    pixels.resize(size_t(w) * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    if (!video && std::getenv("UI_VIDEO")) {
        std::string command = "ffmpeg -v error -y -f rawvideo -pixel_format rgba -video_size " +
                              std::to_string(w) + "x" + std::to_string(h) +
                              " -framerate 60 -i - -vf vflip -an -c:v libx264 -preset veryfast "
                              "-crf 22 logs/transitions.mp4";
        video = popen(command.c_str(), "w");
        require(video, "video encoder", frame);
    }
    if (video)
        require(std::fwrite(pixels.data(), 1, pixels.size(), video) == pixels.size(),
                "write video frame", frame);
    if (destination >= 0) {
        require(pallet3d_active() && pallet3d_covers_frame(ctx),
                "no original 2D frame anywhere in the warp", frame);
        require(pallet3d_input_mask() == 255, "warp neutralizes relative input", frame);
        if (warp) {
            ++frames;
            palettes.insert(bgp);
            auto now = pallet3d_world_frame();
            auto meshes = pallet3d_stats();
            require(now.map == frozen.map && now.camera == frozen.camera,
                    "outgoing camera/scene remain fixed", frame);
            require(meshes.mesh_builds == resident.mesh_builds && meshes.bytes == resident.bytes,
                    "no incoming mesh read during fade", frame);
            bool extreme = bgp == 255 || bgp == 0;
            if (extreme)
                for (size_t i = 0; i < pixels.size(); i++)
                    if (i % 4 != 3)
                        require(pixels[i] == (bgp == 255 ? 0 : 255),
                                "black/white endpoint covers every pixel", frame);
            auto tone = fade::tone(uint8_t(bgp));
            if (reference.empty() && tone.multiply > 0) {
                reference = pixels;
                reference_tone = tone;
            }
            if (!reference.empty()) {
                float ratio = tone.multiply / reference_tone.multiply;
                size_t differences = 0;
                // Alpha-blended shadows and the player x-ray have separate
                // rounding. Allow two byte levels; reject spatial/camera drift.
                for (size_t i = 0; i < pixels.size(); i++)
                    if (i % 4 != 3 && std::abs(float(pixels[i]) -
                                               (reference[i] * ratio +
                                                255 * (tone.add - reference_tone.add * ratio))) >
                                          std::max(2.f, 2 * ratio))
                        ++differences;
                require(differences < pixels.size() / 1000,
                        "frozen scene follows BGP pixel luminance", frame);
            }
            if (bgp != last_bgp) {
                char name[160];
                std::snprintf(name, sizeof(name), "logs/warp-%02d-map-%03d-bgp-%02x.ppm", segments,
                              destination, bgp);
                capture_surface(name);
                last_bgp = bgp;
                std::fprintf(stderr, "[TRANSITION] frame=%d bgp=%02x\n", frame, bgp);
            }
        } else if (state == pallet::View::Overworld) {
            require(map == destination, "arrive in expected map", frame);
            std::fprintf(stderr, "[TRANSITION] end map=%d frame=%d\n", map, frame);
            settled = map;
            destination = -1;
        }
    }
    require(glGetError() == GL_NO_ERROR, "capture GL", frame);
}
inline int run(GBContext *ctx, bool fp, bool white = false) {
    settled = destination = -1;
    segments = frames = 0;
    palettes.clear();
    qa_frame_observer = observe;
    int result = 0;
    if (white)
        result = battle_menus(ctx);
    else if (pallet::read(ctx, pallet::Map) == 0)
        result = interior_journey(ctx, fp);
    else {
        if (fp) {
            SDL_Event e{};
            e.type = SDL_KEYDOWN;
            e.key.keysym.scancode = SDL_SCANCODE_F3;
            pallet3d_event(&e, false);
        }
        result = interior_transitions(ctx);
    }
    qa_frame_observer = nullptr;
    if (video) {
        require(pclose(video) == 0, "finish video", frames);
        video = nullptr;
    }
    require(destination < 0 && segments >= (white ? 1 : 4) && frames > (white ? 18 : 100),
            "complete transition coverage", frames);
    if (white)
        require(palettes.count(0) && palettes.count(0x40) && palettes.count(0x90),
                "measured white fade stages", frames);
    else
        require(palettes.count(0xf9) && palettes.count(0xfe) && palettes.count(0xff),
                "measured black fade stages", frames);
    std::fprintf(stderr,
                 "PASS: %d warps, %d composed frames, fixed camera/meshes, BGP pixel luminance, no "
                 "2D gaps\n",
                 segments, frames);
    return result;
}
inline int reload(GBContext *ctx, const char *path, bool fp = false) {
    QaWalk run{ctx};
    run.wait(30);
    if (fp) {
        SDL_Event e{};
        e.type = SDL_KEYDOWN;
        e.key.keysym.scancode = SDL_SCANCODE_F3;
        pallet3d_event(&e, false);
        run.wait(30);
    }
    auto old = pallet3d_world_frame();
    auto old_meshes = pallet3d_stats();
    run.require(pallet3d_active(), "savestate fade starts from a scene");
    run.require(gb_context_load_state_file(ctx, path), "load a private state in another map");
    pallet3d_state_loaded(ctx);
    int map = pallet::read(ctx, pallet::Map);
    run.require(map != old.map && pallet3d_load_fade(), "successful load starts presentation fade");
    ReadOnlyMemory memory{ctx};
    int outgoing = 0, incoming = 0, partial = 0;
    uint32_t start = SDL_GetTicks();
    bool black = false;
    for (int frame = 0; frame < 200; frame++) {
        bool pending = pallet3d_load_fade();
        run.require(pallet3d_covers_frame(ctx), "load fade covers the original LCD");
        gb_platform_render_frame(gb_get_framebuffer(ctx));
        run.require(pallet3d_active() && memory.unchanged(ctx) && glGetError() == GL_NO_ERROR,
                    "read-only GL load fade");
        auto now = pallet3d_world_frame();
        if (now.map == old.map) {
            ++outgoing;
            run.require(now.camera == old.camera &&
                            pallet3d_stats().mesh_builds == old_meshes.mesh_builds,
                        "outgoing savestate scene retained");
        } else {
            run.require(now.map == map, "loaded scene appears");
            ++incoming;
            if (fp)
                run.require(std::abs(firstperson::angle_delta(
                                pallet3d_camera().yaw,
                                firstperson::facing_yaw(pallet::read(ctx, 0xc109)))) < .0001f,
                            "loaded first-person camera matches engine facing");
            if (incoming > 1 && pallet3d_load_fade())
                ++partial;
            if (incoming == 1) {
                GLint viewport[4];
                glGetIntegerv(GL_VIEWPORT, viewport);
                std::vector<uint8_t> rgba(size_t(viewport[2]) * viewport[3] * 4);
                glReadPixels(0, 0, viewport[2], viewport[3], GL_RGBA, GL_UNSIGNED_BYTE,
                             rgba.data());
                black = true;
                for (size_t i = 0; i < rgba.size(); i++)
                    if (i % 4 != 3 && rgba[i])
                        black = false;
                run.require(black, "scene swaps at completely black frame");
                capture_surface("logs/reload-black.ppm");
            }
        }
        if (pending)
            run.require(pallet3d_input_mask() == 255, "load fade neutral relative controls");
        if (!pallet3d_load_fade())
            break;
        SDL_Delay(5);
    }
    run.require(outgoing > 0 && incoming > 1 && partial >= 2 && black && !pallet3d_load_fade(),
                "load fade reveals destination over multiple intermediate frames");
    capture_surface("logs/reload-destination.ppm");
    std::fprintf(stderr,
                 "PASS: savestate fade %d -> %d, %d outgoing/%d incoming/%d intermediate frames, "
                 "%u ms, black midpoint and unchanged machine\n",
                 old.map, map, outgoing, incoming, partial, uint32_t(SDL_GetTicks() - start));
    return 0;
}
} // namespace transition_qa
