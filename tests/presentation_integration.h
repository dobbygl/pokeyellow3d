#pragma once

namespace presentation_qa {
inline void require(bool ok, const char *message) {
    if (!ok) {
        std::fprintf(stderr, "[CROSSFADE] FAIL: %s\n", message);
        std::exit(43);
    }
}
inline std::vector<uint8_t> render(GBContext *ctx) {
    ReadOnlyMemory memory{ctx};
    gb_platform_render_frame(gb_get_framebuffer(ctx));
    require(memory.unchanged(ctx) && glGetError() == GL_NO_ERROR, "read-only GL presentation");
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    std::vector<uint8_t> image(size_t(viewport[2]) * viewport[3] * 4);
    glReadPixels(0, 0, viewport[2], viewport[3], GL_RGBA, GL_UNSIGNED_BYTE, image.data());
    return image;
}
inline size_t differences(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b,
                          int tolerance = 0) {
    require(a.size() == b.size(), "stable drawable dimensions");
    size_t count = 0;
    for (size_t i = 0; i < a.size(); i++)
        if (i % 4 != 3 && std::abs(int(a[i]) - int(b[i])) > tolerance)
            ++count;
    return count;
}
inline void f2() {
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.keysym.scancode = SDL_SCANCODE_F2;
    require(pallet3d_event(&event, false), "F2 consumed");
}
inline std::vector<uint8_t> settle(GBContext *ctx) {
    for (int i = 0; i < 100; i++) {
        auto image = render(ctx);
        if (!pallet3d_blend().active)
            return image;
        require(pallet3d_covers_frame(ctx) && pallet3d_active() && pallet3d_input_mask() == 255,
                "blend owns the complete frame with neutral relative controls");
        SDL_Delay(8);
    }
    require(false, "bounded crossfade duration");
    return {};
}
inline void exercise(GBContext *ctx, const char *name) {
    for (int i = 0; i < 30; i++)
        render(ctx);
    require(pallet3d_active() && !pallet3d_blend().active, "valid settled source scene");
    auto source = render(ctx);
    std::string prefix = std::string("logs/") + name;
    capture_surface((prefix + "-3d.ppm").c_str());
    f2();
    uint32_t start = SDL_GetTicks();
    std::vector<std::pair<float, std::vector<uint8_t>>> samples;
    std::vector<uint8_t> destination;
    int frames = 0;
    bool midpoint = false;
    for (; frames < 100; frames++) {
        auto image = render(ctx);
        auto blend = pallet3d_blend();
        if (frames == 0)
            require(blend.active && !blend.target_3d && differences(source, image) == 0,
                    "first outgoing frame exactly preserves scene and UI");
        if (!blend.active) {
            destination = std::move(image);
            break;
        }
        require(pallet3d_covers_frame(ctx) && pallet3d_active() && pallet3d_input_mask() == 255,
                "outgoing owns frame and masks relative controls");
        if (!midpoint && blend.progress >= .45f) {
            capture_surface((prefix + "-middle.ppm").c_str());
            midpoint = true;
        }
        if (frames % 3 == 0)
            samples.emplace_back(blend.progress, std::move(image));
        SDL_Delay(8);
    }
    uint32_t elapsed = SDL_GetTicks() - start;
    require(!destination.empty() && frames >= 8 && elapsed >= 180 && elapsed < 800,
            "visible intermediate frames over approximately 200 ms");
    require(!pallet3d_active() && !pallet3d_covers_frame(ctx), "settled 2D returns to the runtime");
    capture_surface((prefix + "-2d.ppm").c_str());
    size_t checked = 0, wrong = 0;
    for (const auto &sample : samples)
        for (size_t i = 0; i < source.size(); i++)
            if (i % 4 != 3) {
                float expected = source[i] * (1 - sample.first) + destination[i] * sample.first;
                ++checked;
                wrong += std::abs(float(sample.second[i]) - expected) > 2;
            }
    std::fprintf(stderr,
                 "[CROSSFADE] %s outgoing frames=%d elapsed=%u pixels=%zu differences=%zu\n", name,
                 frames, elapsed, checked, wrong);
    require(wrong == 0, "every intermediate RGB pixel blends the complete source into native 2D");
    f2();
    auto incoming = render(ctx);
    require(pallet3d_blend().active && pallet3d_blend().target_3d &&
                differences(destination, incoming) == 0,
            "first incoming frame preserves native 2D");
    settle(ctx);
    require(pallet3d_active(), "3D restored");
    capture_surface((prefix + "-returned.ppm").c_str());
    // Reverse a partially blended frame; its exact mixed image becomes the
    // next source, rather than jumping back to either unblended endpoint.
    f2();
    render(ctx);
    for (int i = 0; i < 4; i++) {
        SDL_Delay(12);
        render(ctx);
    }
    auto mixed = render(ctx);
    require(pallet3d_blend().active, "reversal fixture is mid-transition");
    f2();
    auto reversed = render(ctx);
    require(pallet3d_blend().active && differences(mixed, reversed) == 0,
            "reversal is pixel-continuous");
    settle(ctx);
    require(pallet3d_active(), "reversed fade returns to 3D");
}
inline void pause_checks(GBContext *ctx) {
    f2();
    render(ctx);
    SDL_Delay(20);
    render(ctx);
    SDL_Delay(20);
    auto before = render(ctx);
    float progress = pallet3d_blend().progress;
    SDL_Event event{};
    event.type = SDL_WINDOWEVENT;
    event.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    pallet3d_event(&event, false);
    require(SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE, "focus loss shows the system cursor");
    for (int i = 0; i < 3; i++) {
        SDL_Delay(40);
        auto paused = render(ctx);
        require(pallet3d_blend().paused && pallet3d_blend().progress == progress &&
                    differences(before, paused) == 0,
                "focus loss freezes the exact mixed frame and clock");
        require(pallet3d_input_mask() == 255, "focus loss keeps controls neutral");
    }
    event.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
    pallet3d_event(&event, false);
    require(SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE, "focused game hides the system cursor");
    auto resumed = render(ctx);
    require(!pallet3d_blend().paused && pallet3d_blend().progress == progress &&
                differences(before, resumed) == 0,
            "focus return resumes without a time or image jump");
    settle(ctx);
    f2();
    settle(ctx);
    auto escape = [&]() {
        SDL_Event key{};
        key.type = SDL_KEYDOWN;
        key.key.keysym.scancode = SDL_SCANCODE_ESCAPE;
        require(SDL_PushEvent(&key) == 1 && gb_platform_poll_events(ctx), "native Escape key");
        key.type = SDL_KEYUP;
        require(SDL_PushEvent(&key) == 1 && gb_platform_poll_events(ctx), "native Escape release");
    };
    f2();
    render(ctx);
    SDL_Delay(20);
    before = render(ctx);
    progress = pallet3d_blend().progress;
    escape();
    for (int i = 0; i < 3; i++) {
        SDL_Delay(40);
        render(ctx);
        require(pallet3d_blend().paused && pallet3d_blend().progress == progress &&
                    pallet3d_input_mask() == 255,
                "settings preserve the transition and neutral controls");
    }
    capture_surface("logs/settings-paused.ppm");
    escape();
    resumed = render(ctx);
    require(!pallet3d_blend().paused && pallet3d_blend().progress == progress &&
                differences(before, resumed) == 0,
            "closing settings restores the same mixed game image without capturing settings");
    settle(ctx);
    f2();
    settle(ctx);
    std::puts("PASS: focus and native Esc preserve the exact transition, source and elapsed time");
}
inline void fallback_checks(GBContext *ctx) {
    require(battle::normal(ctx), "normal battle fixture for unsupported-mode presentation tests");
    ReadOnlyMemory original{ctx};
    uint8_t type = battle::read(ctx, battle::BattleType), link = battle::read(ctx, battle::Link);
    // Controlled presentation fixtures only. No network peer or special battle
    // is simulated: the original battle LCD stays intact while its mode flags
    // select each unsupported presentation path. Restore all bytes afterward.
    for (auto mode : std::array<std::array<uint8_t, 2>, 3>{{{1, 0}, {2, 0}, {0, 1}}}) {
        for (int i = 0; i < 20; i++)
            render(ctx);
        auto source = render(ctx);
        ctx->wram[battle::BattleType - 0xc000] = mode[0];
        ctx->wram[battle::Link - 0xc000] = mode[1];
        auto first = render(ctx);
        require(pallet3d_blend().active && !pallet3d_blend().target_3d &&
                    differences(source, first) == 0,
                "automatic unsupported fallback preserves its source");
        auto native = settle(ctx);
        require(!pallet3d_active(), "unsupported mode reaches original LCD");
        f2();
        auto unchanged = render(ctx);
        require(!pallet3d_blend().active && differences(native, unchanged) == 0,
                "F2 without a scene preserves the valid 2D image");
        f2();
        unchanged = render(ctx);
        require(!pallet3d_blend().active && differences(native, unchanged) == 0,
                "3D preference waits for a supported scene");
        ctx->wram[battle::BattleType - 0xc000] = type;
        ctx->wram[battle::Link - 0xc000] = link;
        auto returned = render(ctx);
        require(pallet3d_blend().active && pallet3d_blend().target_3d &&
                    differences(native, returned) == 0,
                "automatic return to a valid arena is continuous");
        settle(ctx);
    }
    require(original.unchanged(ctx), "controlled unsupported flags are fully restored");
    std::puts("PASS: automatic special-mode and link-flag fallbacks; original LCD, stable "
              "unsupported F2 and complete fixture restoration");
}
inline int warp(GBContext *ctx, bool fp) {
    if (fp) {
        SDL_Event event{};
        event.type = SDL_KEYDOWN;
        event.key.keysym.scancode = SDL_SCANCODE_F3;
        pallet3d_event(&event, false);
    }
    QaWalk walk{ctx};
    walk.wait(30);
    walk.require(walk.read(pallet::Map) == 0 && walk.read(pallet::X) == 9 &&
                     walk.read(pallet::Y) == 7,
                 "Pallet door fixture");
    walk.move(0, 9, 8, "D");
    walk.move(0, 5, 8, "L");
    walk.input("U");
    for (int i = 0; i < 500 && !pallet3d_warp_overlay(); i++)
        walk.tick();
    require(pallet3d_warp_overlay(), "real door fade is active");
    exercise(ctx, "door-fade");
    for (int i = 0; i < 1000; i++) {
        walk.tick();
        if (walk.read(pallet::Map) == 37 && pallet::view(ctx) == pallet::View::Overworld &&
            !pallet3d_warp_overlay())
            break;
    }
    walk.input(nullptr);
    walk.wait(30);
    require(walk.read(pallet::Map) == 37 && pallet3d_active(),
            "door resumes normally after both F2 directions");
    std::puts(
        "PASS: F2 during a real engine-driven door fade, original warp resumes, no memory writes");
    return 0;
}
inline int run(GBContext *ctx, bool fp) {
    if (fp) {
        SDL_Event event{};
        event.type = SDL_KEYDOWN;
        event.key.keysym.scancode = SDL_SCANCODE_F3;
        pallet3d_event(&event, false);
    }
    exercise(ctx, battle::normal(ctx) ? "battle" : "world");
    pause_checks(ctx);
    if (battle::normal(ctx))
        fallback_checks(ctx);
    if (pallet::view(ctx) == pallet::View::Overworld) {
        QaWalk walk{ctx};
        walk.press("S");
        walk.wait(30);
        require(start_visible(ctx), "real Start menu");
        exercise(ctx, "start");
        for (int i = 0; i < 10 && walk.read(0xcc26) > 0; i++)
            walk.press("U");
        int party = menu_layout::classify(ctx->wram + 0x3a0).regions[0].h == 16 ? 1 : 0;
        for (int i = 0; i < party; i++)
            walk.press("D");
        walk.press("A");
        walk.wait(40);
        require(pallet3d_menu().active && pallet3d_menu().full, "real party menu");
        exercise(ctx, "party");
        for (int i = 0; i < 8 && pallet::view(ctx) != pallet::View::Overworld; i++)
            walk.press("B");
        require(pallet::view(ctx) == pallet::View::Overworld, "original menu returns to the world");
    }
    std::puts("PASS: full-frame F2 fades, exact source and native destination, pixel-linear "
              "intermediates, reversal and read-only memory");
    return 0;
}
} // namespace presentation_qa
