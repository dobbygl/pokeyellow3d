#pragma once

namespace menu_motion_qa {
using presentation_qa::differences;
using presentation_qa::render;
using ui_motion_qa::require;

inline const menu_motion::Panel *start_panel() {
    for (const auto &panel : menu_motion::state.panels)
        if (panel.target && panel.visible && panel.key.region.x == 10 && panel.key.region.y == 0 &&
            panel.key.placement == 0)
            return &panel;
    return nullptr;
}
inline void escape(GBContext *ctx) {
    SDL_Event key{};
    key.type = SDL_KEYDOWN;
    key.key.keysym.scancode = SDL_SCANCODE_ESCAPE;
    require(SDL_PushEvent(&key) == 1 && gb_platform_poll_events(ctx), "native Escape key");
    key.type = SDL_KEYUP;
    require(SDL_PushEvent(&key) == 1 && gb_platform_poll_events(ctx), "native Escape release");
}
inline void opening(QaWalk &walk) {
    walk.input("S");
    for (int i = 0; i < 300 && !start_panel(); ++i)
        walk.tick();
    walk.input(nullptr);
    require(start_panel(), "original engine opens Start");
    walk.wait(3);
    require(start_panel() && start_panel()->amount > 0 && start_panel()->amount < 1,
            "Start is partway through its cycle-driven opening");
}
inline void load(GBContext *ctx, const char *path) {
    require(gb_context_load_state_file(ctx, path), "reload private engine fixture");
    pallet3d_state_loaded(ctx);
    // Render-only frames also finish the existing presentation load fade;
    // they must never advance a menu's emulated-cycle clock.
    for (int i = 0; i < 30; ++i)
        render(ctx);
}
inline int rapid(GBContext *ctx, bool fp) {
    if (fp) {
        SDL_Event event{};
        event.type = SDL_KEYDOWN;
        event.key.keysym.scancode = SDL_SCANCODE_F3;
        pallet3d_event(&event, false);
    }
    QaWalk walk{ctx};
    walk.wait(30);
    for (int trial = 0; trial < 3; ++trial) {
        require(pallet::view(ctx) == pallet::View::Overworld, "rapid-input world");
        walk.input("S");
        for (int i = 0; i < 300 && !start_panel(); ++i)
            walk.tick();
        walk.input(nullptr);
        require(start_panel(), "original Start region before rapid input");
        walk.wait(2);
        require(!menu_motion::enabled || start_panel()->amount < 1,
                "input is delivered before the appearance has completed");
        // Identical cycle/input sequence with cosmetics enabled or disabled.
        // The ROM decides when each key is accepted; presentation never waits.
        walk.input("D");
        walk.wait(2);
        walk.input(nullptr);
        walk.wait(2);
        walk.input("B");
        walk.wait(2);
        walk.input(nullptr);
        walk.wait(30);
        for (int i = 0; i < 8 && pallet::view(ctx) != pallet::View::Overworld; ++i)
            walk.press("B");
        require(pallet::view(ctx) == pallet::View::Overworld, "original Start closes");
        walk.wait(30);
    }
    capture_surface("logs/motion-rapid-complete.ppm");
    std::puts("PASS: three rapid original-input sequences during panel appearance");
    return 0;
}
inline int run(GBContext *ctx, bool fp) {
    require(menu_motion::enabled, "pause/load audit requires animated presentation");
    if (fp) {
        SDL_Event event{};
        event.type = SDL_KEYDOWN;
        event.key.keysym.scancode = SDL_SCANCODE_F3;
        pallet3d_event(&event, false);
    }
    QaWalk walk{ctx};
    walk.wait(30);
    require(pallet::view(ctx) == pallet::View::Overworld, "world fixture");
    require(gb_context_save_state_file(ctx, "logs/motion-world.state"), "save empty world");
    opening(walk);
    auto before = render(ctx);
    float amount = start_panel()->amount;
    uint32_t cycles = ctx->cycles;
    SDL_Event focus{};
    focus.type = SDL_WINDOWEVENT;
    focus.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    pallet3d_event(&focus, false);
    for (int i = 0; i < 3; ++i) {
        SDL_Delay(40);
        auto paused = render(ctx);
        require(menu_motion::state.paused && start_panel()->amount == amount &&
                    ctx->cycles == cycles && differences(before, paused) == 0 &&
                    pallet3d_input_mask() == 255,
                "focus loss freezes exact pixels, phase, cycles and controls");
    }
    focus.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
    pallet3d_event(&focus, false);
    require(differences(before, render(ctx)) == 0 && start_panel()->amount == amount &&
                !menu_motion::state.paused,
            "focus return has no time or image jump");
    escape(ctx);
    for (int i = 0; i < 3; ++i) {
        SDL_Delay(40);
        render(ctx);
        require(menu_motion::state.paused && start_panel()->amount == amount &&
                    ctx->cycles == cycles && pallet3d_input_mask() == 255,
                "native settings freeze the menu and leave controls neutral");
    }
    escape(ctx);
    require(differences(before, render(ctx)) == 0 && start_panel()->amount == amount &&
                !menu_motion::state.paused,
            "closing settings restores the exact partially open panel");
    walk.wait(30);
    require(start_panel() && start_panel()->amount == 1 && start_visible(ctx),
            "original menu finishes normally after both pauses");
    require(gb_context_save_state_file(ctx, "logs/motion-open.state"), "save open menu");
    uint32_t open_cycles = ctx->cycles;
    walk.wait(10);
    load(ctx, "logs/motion-open.state");
    require(start_panel() && start_panel()->amount == 1 && ctx->cycles == open_cycles,
            "backward savestate load immediately restores a complete panel");
    load(ctx, "logs/motion-world.state");
    require(menu_motion::state.panels.empty(), "empty-world load leaves no stale panel");
    require(ctx->cycles < open_cycles, "next load moves the guest clock forward");
    load(ctx, "logs/motion-open.state");
    require(start_panel() && start_panel()->amount == 1 && ctx->cycles == open_cycles,
            "forward savestate load also restores a complete panel without animation");
    load(ctx, "logs/motion-world.state");
    opening(walk);
    walk.wait(30);
    capture_surface("logs/motion-after-pause-load.ppm");
    std::puts("PASS: real Start animation, exact focus/Esc freeze and resume, forward/backward "
              "open-menu loads, empty-world load and subsequent animated opening");
    return 0;
}
} // namespace menu_motion_qa
