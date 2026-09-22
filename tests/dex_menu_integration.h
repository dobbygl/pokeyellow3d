#pragma once

namespace dex_menu_qa {
inline void capture(QaWalk &run, const char *label, bool list = true) {
    if (list && pallet3d_menu_style() == ui_preferences::Style::Integrated)
        run.require(pallet3d_dex_menu().active && !pallet3d_dex_menu().fallback,
                    "original Pokedex list integrates all four regions");
    std::string path = std::string("logs/") + label;
    capture_surface((path + ".ppm").c_str());
    run.require(gb_context_save_state_file(run.ctx, (path + ".state").c_str()),
                "private dex state");
    FILE *f = std::fopen((path + ".tiles").c_str(), "wb");
    run.require(f, "private dex tilemap");
    std::fwrite(run.ctx->wram + 0x3a0, 1, 360, f);
    std::fclose(f);
}
inline void controls(QaWalk &run) {
    if (pallet3d_menu_style() != ui_preferences::Style::Integrated)
        return;
    auto *ctx = run.ctx;
    using presentation_qa::differences;
    using presentation_qa::render;
    auto pixels = render(ctx);
    struct Mutation {
        uint8_t *address;
        const char *name;
    };
    for (auto m :
         {Mutation{ctx->wram + 0x3a0 + 2 * 20 + 14, "separator"},
          Mutation{ctx->wram + 0x3a0 + 2 * 20 + 5, "unknown-tile"},
          Mutation{ctx->wram + 0xc28, "selection"}, Mutation{ctx->vram + 0x1720, "caught-graphic"},
          Mutation{ctx->vram + 0x820, "font"}}) {
        uint8_t saved = *m.address;
        // At row zero a maximum of 255 does not invalidate current selection;
        // make the current selection out of range while keeping it in the dex.
        uint8_t current = ctx->wram[0xc26];
        if (std::strcmp(m.name, "selection") == 0) {
            *m.address = 0;
            ctx->wram[0xc26] = 1;
        } else
            *m.address ^= 1;
        render(ctx);
        run.require(pallet3d_dex_menu().active && pallet3d_dex_menu().fallback,
                    "invalid dex source keeps complete framed LCD");
        std::fprintf(stderr, "[UI-DEX-NEGATIVE] %s full LCD verified\n", m.name);
        *m.address = saved;
        ctx->wram[0xc26] = current;
        run.require(differences(pixels, render(ctx)) == 0 && !pallet3d_dex_menu().fallback,
                    "restored dex source restores identical complete presentation");
    }
    uint32_t cycles = ctx->cycles;
    SDL_Event focus{};
    focus.type = SDL_WINDOWEVENT;
    focus.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    pallet3d_event(&focus, false);
    for (int i = 0; i < 3; ++i) {
        SDL_Delay(20);
        run.require(differences(pixels, render(ctx)) == 0 && ctx->cycles == cycles &&
                        menu_motion::state.paused,
                    "dex freezes without focus");
    }
    focus.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
    pallet3d_event(&focus, false);
    menu_motion_qa::escape(ctx);
    render(ctx);
    run.require(menu_motion::state.paused && ctx->cycles == cycles, "Esc freezes dex decoration");
    menu_motion_qa::escape(ctx);
    run.require(differences(pixels, render(ctx)) == 0, "Esc restores identical dex list");
    run.require(gb_context_save_state_file(ctx, "logs/dex-open.state"), "private open dex save");
    run.wait(8);
    menu_motion_qa::load(ctx, "logs/dex-open.state");
    run.require(ctx->cycles == cycles && pallet3d_dex_menu().active &&
                    !pallet3d_dex_menu().fallback,
                "loaded dex immediately presents complete list");
    for (const auto &panel : menu_motion::state.panels)
        if (panel.target)
            run.require(panel.amount == 1, "loaded device panels snap open");
    std::puts("[DEX-MOTION] PASS focus, Esc and open-menu load");
}
inline int journey(GBContext *ctx, bool fp, bool short_list) {
    QaWalk run{ctx};
    run.wait(30);
    run.require(pallet::view(ctx) == pallet::View::Overworld, "private world before dex");
    int last = short_list ? 5 : 151;
    // Unlock only original bitmap entries. The engine draws names, counters,
    // markers and all cursor/scroll changes; no tilemap or cursor is injected.
    std::fill_n(ctx->wram + 0x12f6, 38, 0);
    ctx->wram[0x12f6] = 1;
    ctx->wram[0x1309] = 3;
    ctx->wram[0x1309 + (last - 1) / 8] |= uint8_t(1 << ((last - 1) % 8));
    item_qa::camera(run, fp);
    auto world = pallet3d_world_frame();
    auto meshes = pallet3d_stats().mesh_builds;
    qa_frame_observer = dex_qa::observe;
    run.press("S");
    for (int i = 0; i < 10 && run.read(0xcc26); ++i)
        run.press("U");
    run.press("A");
    run.wait(60);
    auto verify = [&](int n) {
        auto info = pallet3d_dex();
        int registration = n == 1 ? 2 : (n == 2 || n == last) ? 1 : 0;
        run.require(info.active && info.list && info.number == n &&
                        info.registration == registration && info.seen == 3 && info.caught == 1,
                    "original list selection and bitmaps");
        auto picture = mon_pic::front(ctx->rom, ctx->rom_size, info.species, true);
        auto image = mon_pic::rgba(picture, battle::palette(ctx, info.species), registration == 1);
        if (!registration)
            image = {};
        run.require(info.image == battle::fingerprint(image) && info.cached <= 32,
                    "original selection and colored/seen/absent portrait remain coherent");
        if (pallet3d_menu_style() == ui_preferences::Style::Integrated)
            run.require(pallet3d_dex_menu().active && !pallet3d_dex_menu().fallback,
                        "stable original list uses theme");
    };
    verify(1);
    capture(run, "dex-first");
    controls(run);
    if (!short_list) {
        run.press("R");
        verify(8);
        capture(run, "dex-page-next");
        run.press("L");
        verify(1);
    }
    for (int n = 2; n <= last; ++n) {
        run.press("D", 2);
        verify(n);
        if (n == 2)
            capture(run, "dex-seen");
        if (n == 3)
            capture(run, "dex-absent");
    }
    capture(run, "dex-last");
    run.press("D");
    verify(last);
    for (int n = last - 1; n >= 1; --n) {
        run.press("U", 2);
        verify(n);
    }
    capture(run, "dex-return-first");
    run.press("A");
    run.wait(40);
    capture(run, "dex-side");
    run.press("A");
    run.wait(140); // DATA keeps its existing device composition.
    run.require(dex_qa::data(ctx) && pallet3d_dex().active && !pallet3d_dex().list,
                "original DATA navigation retains verified portrait");
    capture(run, "dex-data", false);
    for (int i = 0; i < 8 && !battle::live_return(ctx, 0x4003d, 0x4140); ++i)
        run.press("B");
    run.wait(40);
    verify(1);
    run.press("A");
    run.press("D");
    run.press("A");
    run.wait(120);
    capture(run, "dex-cry");
    verify(1);
    run.press("B");
    run.press("B");
    run.press("B");
    run.wait(40);
    qa_frame_observer = nullptr;
    run.require(pallet::view(ctx) == pallet::View::Overworld && !pallet3d_dex().active &&
                    pallet3d_stats().mesh_builds == meshes &&
                    pallet3d_world_frame().camera == world.camera && pallet3d_firstperson() == fp,
                "dex restores world meshes and camera");
    std::puts("PASS: original full dex short/long list, flags, scroll, DATA/CRY and return");
    return 0;
}
} // namespace dex_menu_qa
