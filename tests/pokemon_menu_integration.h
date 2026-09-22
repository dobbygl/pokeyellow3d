#pragma once

namespace pokemon_qa {
inline int pc(GBContext *ctx, bool fp) {
    pc_qa::Session session(ctx, fp);
    auto &walk = session.run;
    walk.require(walk.read(0xd162) >= 2, "two Pokemon before PC summary checks");
    auto summary = [&](const char *prefix, int location) {
        walk.press("D"); // Original action menu: Deposit/Withdraw, Stats, Cancel.
        walk.press("A");
        walk.wait(80);
        walk.require(walk.read(0xcc49) == location, "original summary source record location");
        verify_menu_overlay(walk, (std::string(prefix) + "-stats").c_str(), 2);
        if (pallet3d_menu_style() == ui_preferences::Style::Integrated)
            walk.require(pallet3d_pokemon_menu().active && !pallet3d_pokemon_menu().fallback,
                         "PC summary integrates over the original storage shelf");
        walk.press("A");
        walk.wait(60);
        verify_menu_overlay(walk, (std::string(prefix) + "-moves").c_str(), 2);
        if (pallet3d_menu_style() == ui_preferences::Style::Integrated)
            walk.require(pallet3d_pokemon_menu().active && !pallet3d_pokemon_menu().fallback,
                         "PC move page reads persistent experience");
        walk.press("B");
        walk.wait(60);
        for (int i = 0; i < 8 && !pc_qa::bill_main(ctx); ++i)
            walk.press("B");
        session.wait_bill();
    };
    session.choose(1);
    walk.press("D");
    walk.press("A");
    walk.wait(35);
    summary("pokemon-pc-party", 0);
    int stored = walk.read(0xda7f);
    session.deposit(1);
    session.choose(0);
    for (int i = 0; i < stored; ++i)
        walk.press("D");
    walk.press("A");
    walk.wait(35);
    summary("pokemon-pc-box", 2);
    session.close();
    std::puts("PASS: original PC party/box summaries, persistent experience and both pages");
    return 0;
}
inline void negative(QaWalk &run) {
    if (pallet3d_menu_style() != ui_preferences::Style::Integrated)
        return;
    auto *ctx = run.ctx;
    auto probe = [&](uint8_t &value, uint8_t changed, const char *name) {
        uint8_t saved = value;
        value = changed;
        ReadOnlyMemory before{ctx};
        gb_platform_render_frame(gb_get_framebuffer(ctx));
        run.require(before.unchanged(ctx) && pallet3d_pokemon_menu().active &&
                        pallet3d_pokemon_menu().fallback,
                    "unsupported Pokemon screen preserves complete LCD and source memory");
        capture_surface((std::string("logs/pokemon-negative-") + name + ".ppm").c_str());
        value = saved;
        ReadOnlyMemory restored{ctx};
        gb_platform_render_frame(gb_get_framebuffer(ctx));
        run.require(restored.unchanged(ctx) && !pallet3d_pokemon_menu().fallback,
                    "restored Pokemon source immediately returns to integrated presentation");
    };
    probe(ctx->wram[0x3a0 + 8 * 20], 0x7f, "border");
    probe(ctx->wram[0x3a0 + 9], 0x61, "unknown-tile");
    probe(ctx->vram[0x1000], uint8_t(ctx->vram[0x1000] ^ 1), "portrait");
    probe(ctx->vram[0x1740], uint8_t(ctx->vram[0x1740] ^ 1), "number-graphic");
    probe(ctx->vram[0x800 + (0x8f - 0x80) * 16], uint8_t(ctx->vram[0x800 + (0x8f - 0x80) * 16] ^ 1),
          "font");
}
inline void pause_and_load(QaWalk &run) {
    if (pallet3d_menu_style() != ui_preferences::Style::Integrated)
        return;
    using presentation_qa::differences;
    using presentation_qa::render;
    auto *ctx = run.ctx;
    run.wait(30);
    auto before = render(ctx);
    auto cycles = ctx->cycles;
    SDL_Event focus{};
    focus.type = SDL_WINDOWEVENT;
    focus.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    pallet3d_event(&focus, false);
    for (int i = 0; i < 3; ++i) {
        SDL_Delay(30);
        run.require(differences(before, render(ctx)) == 0 && ctx->cycles == cycles &&
                        menu_motion::state.paused,
                    "Pokemon menu pixels and animation freeze without focus");
    }
    focus.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
    pallet3d_event(&focus, false);
    run.require(differences(before, render(ctx)) == 0,
                "focus resume preserves Pokemon menu pixels");
    menu_motion_qa::escape(ctx);
    for (int i = 0; i < 3; ++i) {
        SDL_Delay(30);
        render(ctx);
        run.require(menu_motion::state.paused && ctx->cycles == cycles,
                    "settings freeze Pokemon menu guest clock");
    }
    menu_motion_qa::escape(ctx);
    run.require(differences(before, render(ctx)) == 0, "closing settings restores Pokemon menu");
    run.require(gb_context_save_state_file(ctx, "logs/pokemon-open.state"),
                "save open Pokemon menu");
    run.wait(8);
    menu_motion_qa::load(ctx, "logs/pokemon-open.state");
    run.require(ctx->cycles == cycles && pallet3d_pokemon_menu().active &&
                    !pallet3d_pokemon_menu().fallback,
                "loaded Pokemon menu resumes without advancing engine");
    for (const auto &panel : menu_motion::state.panels)
        if (panel.target)
            run.require(panel.amount == 1, "loaded full-screen panels snap open");
}
inline int run(GBContext *ctx, bool fp, bool six) {
    QaWalk walk{ctx};
    walk.wait(30);
    walk.require(pallet::view(ctx) == pallet::View::Overworld && walk.read(0xd162) >= 2,
                 "original two-Pokemon world fixture");
    if (six) {
        // Only this disposable fixture is changed; the original engine paints
        // every tested name, status, HP number, level and field-move option.
        // Record size/name lengths verified in wram.asm and pokemon_data.asm.
        std::array<uint8_t, 44> mon{};
        std::array<uint8_t, 11> name{};
        std::copy_n(ctx->wram + 0x116a, mon.size(), mon.begin());
        std::copy_n(ctx->wram + 0x12b4, name.size(), name.begin());
        int max_hp = (mon[34] << 8) | mon[35];
        int hp[]{0, 1, max_hp / 3, max_hp / 2, max_hp - 1, max_hp};
        int status[]{0, 8, 16, 32, 64, 1};
        for (int i = 0; i < 6; ++i) {
            auto *record = ctx->wram + 0x116a + i * 44;
            std::copy(mon.begin(), mon.end(), record);
            std::copy(name.begin(), name.end(), ctx->wram + 0x12b4 + i * 11);
            ctx->wram[0x1163 + i] = mon[0];
            record[1] = uint8_t(hp[i] >> 8);
            record[2] = uint8_t(hp[i]);
            record[4] = uint8_t(status[i]);
            // CUT and SURF, original constants 0F/39: exercise the wider,
            // taller action window without ever executing a field move.
            record[8] = 0x0f;
            record[9] = 0x39;
            if (i == 5) {
                record[3] = record[33] = 100;
                int xp = battle::experience_at(ctx, mon[0], 100);
                record[14] = uint8_t(xp >> 16);
                record[15] = uint8_t(xp >> 8);
                record[16] = uint8_t(xp);
            }
        }
        ctx->wram[0x1162] = 6;
        ctx->wram[0x1169] = 0xff;
    }
    if (fp) {
        SDL_Event event{};
        event.type = SDL_KEYDOWN;
        event.key.keysym.scancode = SDL_SCANCODE_F3;
        pallet3d_event(&event, false);
        walk.wait(30);
    }
    qa_frame_observer = observe_menu_frame;
    walk.press("S");
    walk.require(start_visible(ctx), "open original Start menu");
    for (int i = 0; i < 10 && walk.read(0xcc26); ++i)
        walk.press("U");
    walk.press("D");
    walk.press("A");
    walk.wait(60);
    int count = walk.read(0xd162);
    for (int slot = 0; slot < count; ++slot) {
        for (int i = 0; i < 10 && walk.read(0xcc26) > slot; ++i)
            walk.press("U");
        for (int i = 0; i < 10 && walk.read(0xcc26) < slot; ++i)
            walk.press("D");
        walk.require(walk.read(0xcc26) == slot, "original party cursor selects requested record");
        std::string prefix = "pokemon-" + std::to_string(slot);
        verify_menu_overlay(walk, (prefix + "-party").c_str(), 2);
        if (pallet3d_menu_style() == ui_preferences::Style::Integrated)
            walk.require(pallet3d_pokemon_menu().active && !pallet3d_pokemon_menu().fallback,
                         "supported party row is integrated");
        walk.press("A");
        walk.wait(20);
        verify_menu_overlay(walk, (prefix + "-actions").c_str(), 2);
        if (pallet3d_menu_style() == ui_preferences::Style::Integrated)
            walk.require(pallet3d_pokemon_menu().active && !pallet3d_pokemon_menu().fallback,
                         "original field-move action geometry is integrated");
        int stats = walk.read(0xcc28) - 2;
        for (int i = 0; i < stats; ++i)
            walk.press("D");
        walk.press("A");
        walk.wait(80);
        verify_menu_overlay(walk, (prefix + "-stats").c_str(), 2);
        if (pallet3d_menu_style() == ui_preferences::Style::Integrated)
            walk.require(pallet3d_pokemon_menu().active && !pallet3d_pokemon_menu().fallback,
                         "supported summary is integrated");
        if (!slot) {
            negative(walk);
            pause_and_load(walk);
        }
        walk.press("A");
        walk.wait(60);
        verify_menu_overlay(walk, (prefix + "-moves").c_str(), 2);
        if (pallet3d_menu_style() == ui_preferences::Style::Integrated)
            walk.require(pallet3d_pokemon_menu().active && !pallet3d_pokemon_menu().fallback,
                         "supported move/experience page is integrated");
        walk.press("B");
        walk.wait(60);
    }
    walk.press("B");
    walk.press("B");
    walk.wait(30);
    qa_frame_observer = nullptr;
    std::puts("PASS: original party, field actions, HP/status/level and both summary pages; "
              "readonly frames");
    return 0;
}
} // namespace pokemon_qa
