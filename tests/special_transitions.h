#pragma once

// Test-only field-move grants. Every menu, animation and map load after the
// grant executes in the original engine; no presentation module mutates it.
namespace special_transition_qa {
inline void hold_forward(GBContext *ctx) {
    if (!pallet3d_firstperson())
        return;
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.keysym.scancode = SDL_SCANCODE_W;
    presentation_qa::require(pallet3d_event(&event, false), "relative W held before transition");
    pallet3d_poll_controls(ctx, false);
    presentation_qa::require(pallet3d_input_mask() != 255, "relative input is initially active");
}
inline void key(GBContext *ctx, SDL_Scancode code) {
    SDL_Event e{};
    e.type = SDL_KEYDOWN;
    e.key.keysym.scancode = code;
    presentation_qa::require(SDL_PushEvent(&e) == 1 && gb_platform_poll_events(ctx), "SDL key");
    e.type = SDL_KEYUP;
    presentation_qa::require(SDL_PushEvent(&e) == 1 && gb_platform_poll_events(ctx), "SDL release");
}
inline int load_pause(GBContext *ctx, const char *path, bool fp) {
    using namespace presentation_qa;
    if (fp)
        key(ctx, SDL_SCANCODE_F3);
    for (int i = 0; i < 30; i++)
        render(ctx);
    const int origin = pallet3d_world_frame().map;
    hold_forward(ctx);
    require(gb_context_load_state_file(ctx, path), "load another private map");
    pallet3d_state_loaded(ctx);
    require(pallet3d_load_fade(), "load fade begins");
    int pauses = 0;
    for (int incoming = 0; incoming < 2; incoming++) {
        for (int frame = 0; frame < 100; frame++) {
            render(ctx);
            if ((pallet3d_world_frame().map != origin) == bool(incoming))
                break;
            SDL_Delay(5);
        }
        require(pallet3d_load_fade(), "both halves of load fade observed");
        for (bool settings : {false, true}) {
            auto before = render(ctx);
            auto focus = [&](bool gained) {
                SDL_Event event{};
                event.type = SDL_WINDOWEVENT;
                event.window.event =
                    gained ? SDL_WINDOWEVENT_FOCUS_GAINED : SDL_WINDOWEVENT_FOCUS_LOST;
                pallet3d_event(&event, false);
            };
            if (settings)
                key(ctx, SDL_SCANCODE_ESCAPE);
            else
                focus(false);
            render(ctx);
            for (int i = 0; i < 3; i++) {
                SDL_Delay(35);
                auto paused = render(ctx);
                require(pallet3d_load_fade() && pallet3d_input_mask() == 255,
                        "pause preserves load fade and neutral input");
                if (!settings)
                    require(differences(before, paused) == 0,
                            "load fade freezes exact image on focus loss");
            }
            SDL_Delay(35);
            if (settings)
                key(ctx, SDL_SCANCODE_ESCAPE);
            else
                focus(true);
            auto resumed = render(ctx);
            require(differences(before, resumed) == 0,
                    "resuming load fade excludes paused time and restores exact game image");
            ++pauses;
        }
    }
    for (int i = 0; i < 100 && pallet3d_load_fade(); i++) {
        SDL_Delay(5);
        render(ctx);
    }
    require(!pallet3d_load_fade() && pallet3d_active() && pallet3d_input_mask() == 255,
            "load fade completes with no held relative controls");
    capture_surface("logs/load-pause-destination.ppm");
    std::printf(
        "PASS: %d pauses during both halves of state load, exact resume and read-only state\n",
        pauses);
    return 0;
}
inline int stale_battle(GBContext *ctx, bool fp) {
    using namespace presentation_qa;
    QaWalk walk{ctx};
    if (fp)
        key(ctx, SDL_SCANCODE_F3);
    walk.wait(30);
    int origin = walk.read(pallet::Map), origin_x = walk.read(pallet::X),
        origin_y = walk.read(pallet::Y);
    require(origin == 0 || origin == 12, "Pallet or Route 1 stale-scene fixture");
    hold_forward(ctx);
    f2();
    settle(ctx);
    walk.render_enabled = false;
    if (origin == 0) {
        walk.move(0, 9, 2, "U");
        walk.move(0, 10, 2, "R");
    }
    walk.input("U");
    for (int i = 0; i < 4000 && !battle_transition::entry(ctx); i++) {
        if (walk.read(pallet::Battle))
            walk.input(nullptr);
        else if (walk.read(pallet::Map) == 12 && !walk.read(pallet::Walk)) {
            if (walk.read(pallet::Y) <= 28)
                walk.input("D");
            else if (walk.read(pallet::Y) >= 32)
                walk.input("U");
        }
        walk.tick();
    }
    walk.input(nullptr);
    require(battle_transition::entry(ctx), "original encounter reaches its entry wipe in 2D");
    require(walk.read(pallet::Map) != origin || walk.read(pallet::X) != origin_x ||
                walk.read(pallet::Y) != origin_y,
            "real movement invalidates retained world");
    f2();
    walk.render_enabled = true;
    render(ctx);
    int native_frames = 0;
    for (int i = 0; i < 2500 && !battle_menu_visible(ctx); i++) {
        auto transition = pallet3d_battle_transition();
        require(!transition.active || transition.arena,
                "battle entry never reuses stale world/camera");
        require(pallet3d_input_mask() == 255, "battle fallback has neutral controls");
        if (!transition.active)
            ++native_frames;
        if (i % 35 == 0)
            walk.input("A");
        if (i % 35 == 6)
            walk.input(nullptr);
        walk.tick();
    }
    walk.input(nullptr);
    settle(ctx);
    require(native_frames > 0 && battle_menu_visible(ctx) && pallet3d_battle_transition().arena,
            "original entry remains visible until a verified 3D arena is ready");
    capture_surface("logs/stale-map-battle.ppm");
    std::printf("PASS: F2-off movement from map %d; %d native entry frames, then valid 3D arena\n",
                origin, native_frames);
    return 0;
}
inline int transport_frames = 0;
inline std::set<int> transport_modes;
inline void observe_transport(GBContext *ctx, int frame) {
    using namespace presentation_qa;
    if (!pallet3d_active() || !pallet3d_covers_frame(ctx) || pallet3d_blend().active ||
        pallet3d_warp_overlay())
        std::fprintf(stderr, "[TRANSPORT] frame=%d view=%d mode=%d bgp=%02x pc=%04x sp=%04x\n",
                     frame, int(pallet::view(ctx)), pallet::read(ctx, 0xd6ff), ctx->io[0x47],
                     ctx->pc, ctx->sp);
    require(pallet3d_active() && pallet3d_covers_frame(ctx) && !pallet3d_blend().active &&
                !pallet3d_warp_overlay(),
            "mount/dismount stays 3D without a transition frame");
    if (pallet::view(ctx) != pallet::View::Overworld)
        require(pallet3d_input_mask() == 255, "transport menus neutralize controls");
    ++transport_frames;
    transport_modes.insert(pallet::read(ctx, 0xd6ff));
}
inline void choose_start(QaWalk &walk, int offset) {
    walk.press("S");
    walk.require(start_visible(walk.ctx), "real Start menu");
    for (int i = 0; i < 10 && walk.read(0xcc26) > 0; i++)
        walk.press("U");
    int party = menu_layout::classify(walk.ctx->wram + 0x3a0).regions[0].h == 16 ? 1 : 0;
    for (int i = 0; i < party + offset; i++)
        walk.press("D");
    walk.press("A");
    walk.wait(40);
}
inline int transport(GBContext *ctx, bool bike, bool fp) {
    QaWalk walk{ctx};
    if (fp)
        key(ctx, SDL_SCANCODE_F3);
    walk.wait(30);
    walk.require(walk.read(pallet::Map) == 0 && !walk.read(0xd6ff), "unmounted Pallet fixture");
    qa_frame_observer = observe_transport;
    hold_forward(ctx);
    auto mount = [&]() {
        choose_start(walk, bike ? 1 : 0);
        walk.press("A");
        walk.wait(40);
        capture_surface("logs/transport-use.ppm");
        walk.press("A");
        for (int i = 0; i < 1500; i++) {
            walk.tick();
            if (pallet::view(ctx) == pallet::View::Overworld && !menu_state::running(ctx))
                break;
            if (i % 90 == 89)
                walk.press("A");
        }
        walk.input(nullptr);
        walk.wait(30);
    };
    mount();
    walk.require(walk.read(0xd6ff) == (bike ? 1 : 2), "original engine mounts transport");
    capture_surface("logs/transport-mounted.ppm");
    if (bike) {
        walk.move(0, 9, 3, "U");
        mount();
    } else {
        walk.move(0, 6, 15, "D");
        walk.move(0, 6, 13, "U");
    }
    walk.wait(30);
    qa_frame_observer = nullptr;
    walk.require(!walk.read(0xd6ff) && transport_modes.size() == 2 && transport_frames > 100,
                 "original engine dismounts with both modes covered");
    capture_surface("logs/transport-dismounted.ppm");
    std::printf("PASS: %s mount, movement and dismount; %d frames with no transition\n",
                bike ? "bike" : "surf", transport_frames);
    return 0;
}
inline FILE *trace = nullptr;
inline int previous_map = -1, previous_view = -1, previous_bgp = -1;
inline bool started = false, arrived = false;
inline int composed_frames = 0;
inline PalletWorldFrameInfo outgoing{};
inline std::set<int> palettes;
inline void observe(GBContext *ctx, int frame) {
    const int map = pallet::read(ctx, pallet::Map), view = int(pallet::view(ctx));
    const int bgp = ctx->io[0x47];
    if (!started && fade::field_departure(ctx)) {
        started = true;
        outgoing = pallet3d_world_frame();
    }
    if (started) {
        if (!pallet3d_active() || !pallet3d_covers_frame(ctx) || pallet3d_blend().active)
            std::fprintf(stderr,
                         "[SPECIAL] gap frame=%d map=%d flags=%02x bank=%02x pc=%04x sp=%04x\n",
                         frame, map, ctx->wram[0x1731], ctx->hram[0x38], ctx->pc, ctx->sp);
        presentation_qa::require(pallet3d_active() && pallet3d_covers_frame(ctx) &&
                                     !pallet3d_blend().active,
                                 "field travel stays entirely 3D without a fallback blend");
        presentation_qa::require(pallet3d_input_mask() == 255,
                                 "field travel never retains relative input");
        if (pallet3d_warp_overlay()) {
            ++composed_frames;
            palettes.insert(bgp);
            auto now = pallet3d_world_frame();
            if (now.map != outgoing.map && !arrived) {
                presentation_qa::require(fade::field_arrival(ctx) && bgp == 0,
                                         "destination camera snaps at the white midpoint");
                arrived = true;
                if (pallet3d_firstperson()) {
                    auto camera = pallet3d_camera();
                    auto player = pallet::world_player(ctx);
                    presentation_qa::require(std::abs(camera.x - player[0]) < .0001f &&
                                                 std::abs(camera.z - player[1]) < .0001f,
                                             "first destination frame snaps to live player");
                }
            }
            if (!arrived)
                presentation_qa::require(now.map == outgoing.map && now.camera == outgoing.camera,
                                         "departure retains its exact camera");
        }
    }
    std::fprintf(trace, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%04x,%04x,", frame, map,
                 pallet::read(ctx, pallet::X), pallet::read(ctx, pallet::Y), view, bgp,
                 pallet3d_active(), pallet3d_warp_overlay(), pallet3d_input_mask(), ctx->pc,
                 ctx->sp);
    for (int a = ctx->sp; a >= 0xd000 && a < 0xdfff; a += 2)
        std::fprintf(trace, "%04x ", pallet::read(ctx, a) | (pallet::read(ctx, a + 1) << 8));
    std::fputc('\n', trace);
    if (map != previous_map || view != previous_view || bgp != previous_bgp) {
        char path[100];
        std::snprintf(path, sizeof(path), "logs/frame-%04d-map-%d-view-%d-bgp-%02x.ppm", frame, map,
                      view, bgp);
        capture_surface(path);
        std::fprintf(stderr, "[SPECIAL] frame=%d map=%d view=%d bgp=%02x warp=%d pc=%04x\n", frame,
                     map, view, bgp, pallet3d_warp_overlay(), ctx->pc);
        previous_map = map;
        previous_view = view;
        previous_bgp = bgp;
    }
}
inline int travel(GBContext *ctx, const char *move, bool fp) {
    QaWalk walk{ctx};
    const bool fly = !std::strcmp(move, "fly"), dig = !std::strcmp(move, "dig");
    walk.require(fly || dig || !std::strcmp(move, "teleport"), "known field move");
    walk.require(walk.read(pallet::Map) == (dig ? 46 : 1), "field move origin");
    walk.expected_respawn = 0;
    // A private early party receives a single move and the Fly badge/visited
    // flag. These are setup only, before per-frame read-only guards begin.
    ctx->wram[0x1172] = fly ? 19 : dig ? 91 : 100;
    for (int i = 1; i < 4; i++)
        ctx->wram[0x1172 + i] = 0;
    ctx->wram[0x1355] |= 4;
    ctx->wram[0x170a] |= 3;
    ctx->wram[0x1718] = 0;
    if (fp) {
        SDL_Event event{};
        event.type = SDL_KEYDOWN;
        event.key.keysym.scancode = SDL_SCANCODE_F3;
        pallet3d_event(&event, false);
    }
    walk.wait(30);
    trace = std::fopen("logs/travel.csv", "w");
    walk.require(trace, "travel trace");
    qa_frame_observer = observe;
    hold_forward(ctx);
    walk.press("S");
    walk.require(start_visible(ctx), "real Start menu");
    for (int i = 0; i < 10 && walk.read(0xcc26) > 0; i++)
        walk.press("U");
    if (menu_layout::classify(ctx->wram + 0x3a0).regions[0].h == 16)
        walk.press("D");
    walk.press("A");
    walk.wait(40);
    walk.press("A");
    walk.wait(40);
    capture_surface("logs/field-menu.ppm");
    walk.press("A");
    walk.wait(180);
    capture_surface("logs/field-selected.ppm");
    // Pallet is the first destination in the original Fly list.
    walk.expected_respawn = 0;
    walk.press("A");
    for (int i = 0; i < 1600; i++) {
        walk.tick();
        if (walk.read(pallet::Map) == 0 && pallet::view(ctx) == pallet::View::Overworld &&
            !pallet3d_warp_overlay())
            break;
        if (i % 100 == 99)
            walk.press("A");
    }
    walk.input(nullptr);
    walk.wait(30);
    qa_frame_observer = nullptr;
    std::fclose(trace);
    trace = nullptr;
    capture_surface("logs/destination.ppm");
    walk.require(walk.read(pallet::Map) == 0, "field move reaches Pallet through engine");
    walk.require(started && arrived && composed_frames > 40 && palettes.count(0) &&
                     palettes.count(0x40) && palettes.count(0x90),
                 "complete white fade and destination coverage");
    std::printf("PASS: original-engine %s reaches Pallet, %d composed frames, white midpoint, "
                "camera snap and neutral input\n",
                move, composed_frames);
    return 0;
}
} // namespace special_transition_qa
