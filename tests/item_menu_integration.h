#pragma once

// Fixture writes happen only in this private QA driver. Every menu, cursor,
// quantity change, transaction and swap afterwards runs in the original game.
namespace item_qa {
inline void camera(QaWalk &run, bool fp) {
    if (fp) {
        SDL_Event event{};
        event.type = SDL_KEYDOWN;
        event.key.keysym.scancode = SDL_SCANCODE_F3;
        pallet3d_event(&event, false);
    }
    run.wait(30);
}
inline void inventory(GBContext *ctx, int count) {
    // item_constants.asm: balls, medicines, stones, vitamins, Town Map, TM01.
    constexpr uint8_t ids[]{4,  20, 11, 12, 13, 14, 15, 16, 17, 18,
                            19, 29, 30, 32, 33, 34, 35, 36, 5,  201};
    std::fill_n(ctx->wram + 0x131c, 42, 0);
    ctx->wram[0x131c] = uint8_t(count);
    for (int i = 0; i < count; ++i) {
        ctx->wram[0x131d + i * 2] = ids[i];
        ctx->wram[0x131e + i * 2] = uint8_t(i == 0 ? 10 : i == 1 ? 99 : i == 2 ? 1 : 17);
    }
    ctx->wram[0x131d + count * 2] = 255;
    ctx->wram[0xc2c] = 0; // Original saved bag selection, before opening a menu.
    ctx->wram[0xc36] = 0;
    ctx->wram[0x1346] = 9;
    ctx->wram[0x1347] = ctx->wram[0x1348] = 0x99;
}
inline void capture(QaWalk &run, const std::string &label) {
    if (pallet3d_menu_style() == ui_preferences::Style::Integrated) {
        auto shown = pallet3d_full_menu();
        run.require(shown.active && !shown.fallback,
                    "supported original item menu uses complete integrated panels");
        full_menu_negative_controls(run);
    }
    std::string path = "logs/" + label;
    capture_surface((path + ".ppm").c_str());
    run.require(gb_context_save_state_file(run.ctx, (path + ".state").c_str()),
                "private original item menu state");
    FILE *file = std::fopen((path + ".tiles").c_str(), "wb");
    run.require(file, "private original item tilemap");
    std::fwrite(run.ctx->wram + 0x3a0, 1, 360, file);
    std::fclose(file);
    std::fprintf(stderr, "[ITEM-CAPTURE] %s current=%d scroll=%d max=%d quantity=%d sp=%04x\n",
                 label.c_str(), run.read(0xcc26), run.read(0xcc36), run.read(0xcc28),
                 run.read(0xcf95), run.ctx->sp);
}
inline void pause_and_load(QaWalk &run) {
    if (pallet3d_menu_style() != ui_preferences::Style::Integrated)
        return;
    using presentation_qa::differences;
    using presentation_qa::render;
    auto *ctx = run.ctx;
    auto pixels = render(ctx);
    auto cycles = ctx->cycles;
    SDL_Event focus{};
    focus.type = SDL_WINDOWEVENT;
    focus.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    pallet3d_event(&focus, false);
    for (int i = 0; i < 3; ++i) {
        SDL_Delay(20);
        run.require(differences(pixels, render(ctx)) == 0 && ctx->cycles == cycles &&
                        menu_motion::state.paused,
                    "complete item menu freezes pixels and cycles without focus");
    }
    focus.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
    pallet3d_event(&focus, false);
    menu_motion_qa::escape(ctx);
    render(ctx);
    run.require(menu_motion::state.paused && ctx->cycles == cycles,
                "Esc freezes complete item menu animation");
    menu_motion_qa::escape(ctx);
    run.require(differences(pixels, render(ctx)) == 0, "Esc restores complete item menu pixels");
    run.require(gb_context_save_state_file(ctx, "logs/item-open.state"), "private open menu save");
    run.wait(8);
    menu_motion_qa::load(ctx, "logs/item-open.state");
    run.require(ctx->cycles == cycles && pallet3d_full_menu().active &&
                    !pallet3d_full_menu().fallback,
                "loaded item menu integrates immediately without advancing game");
    for (const auto &panel : menu_motion::state.panels)
        if (panel.target)
            run.require(panel.amount == 1, "loaded item menu panels snap open");
    std::puts("[ITEM-MOTION] PASS focus, Esc and open-menu load");
}
inline void open_bag(QaWalk &run) {
    run.press("S");
    run.require(start_visible(run.ctx), "original Start before bag");
    for (int i = 0; i < 10 && run.read(0xcc26); ++i)
        run.press("U");
    run.press("D");
    run.press("D");
    run.press("A");
    run.wait(60);
    run.require(battle::live_return(run.ctx, 0x11ed2, 0x2ae0), "original bag list call");
}
inline void close(QaWalk &run) {
    for (int i = 0; i < 20 && menu_state::running(run.ctx); ++i) {
        run.press("B");
        run.wait(20);
    }
    run.require(!menu_state::running(run.ctx) && pallet::view(run.ctx) == pallet::View::Overworld,
                "original menu closes to world");
    qa_frame_observer = nullptr;
}
inline int bag(GBContext *ctx, bool fp, int count) {
    QaWalk run{ctx};
    run.wait(30);
    run.require(pallet::view(ctx) == pallet::View::Overworld, "world before private bag fixture");
    inventory(ctx, count);
    camera(run, fp);
    qa_frame_observer = observe_menu_frame;
    open_bag(run);
    capture(run, "bag-first");
    pause_and_load(run);
    if (!count) {
        run.press("A");
        run.require(start_visible(ctx), "empty bag Cancel returns to Start");
        close(run);
        std::puts("PASS: original empty bag and Cancel");
        return 0;
    }
    if (count > 2) {
        for (int i = 0; i < count - 1; ++i)
            run.press("D");
        run.require(run.read(0xcc26) + run.read(0xcc36) == count - 1,
                    "bag reaches last original item across scroll boundaries");
        capture(run, "bag-last");
        run.press("D");
        run.require(run.read(0xcc26) + run.read(0xcc36) == count,
                    "bag reaches original Cancel after last item");
        capture(run, "bag-cancel");
        for (int i = 0; i < count; ++i)
            run.press("U");
        run.require(run.read(0xcc26) == 0 && run.read(0xcc36) == 0,
                    "bag returns to its original first row");
    }
    run.press("T");
    run.press("D");
    capture(run, "bag-swap-selected");
    run.press("T");
    run.require(run.read(0xd31d) == 20 && run.read(0xd31f) == 4,
                "Select swaps original item records");
    capture(run, "bag-swapped");
    run.press("T");
    run.press("U");
    run.press("T");
    run.require(run.read(0xd31d) == 4 && run.read(0xd31f) == 20,
                "original swap restores the first two records");
    run.press("A");
    run.wait(40);
    capture(run, "bag-actions");
    run.press("A"); // Poke Ball cannot be used in the overworld.
    run.wait(180);
    capture(run, "bag-use-message");
    for (int i = 0; i < 12 && !battle::live_return(ctx, 0x11ed2, 0x2ae0); ++i)
        run.press("B");
    run.wait(40);
    run.require(battle::live_return(ctx, 0x11ed2, 0x2ae0), "use message returns to original bag");
    run.press("A");
    run.press("D");
    run.press("A");
    run.wait(40);
    run.require(run.read(0xcf95) == 1, "original Toss quantity starts at one");
    run.press("U");
    run.require(run.read(0xcf95) == 2, "original Toss changes quantity");
    capture(run, "bag-quantity");
    run.press("A");
    run.wait(180);
    for (int i = 0; i < 8 && battle::tile(ctx, 14, 7) != 0x79; ++i)
        run.press("A");
    capture(run, "bag-toss-confirm");
    for (int i = 0; i < 12 && run.read(0xd31e) == 10; ++i) {
        run.press("A");
        run.wait(40);
    }
    run.require(run.read(0xd31e) == 8, "original Toss removes exactly two Poke Balls");
    for (int i = 0; i < 12 && !battle::live_return(ctx, 0x11ed2, 0x2ae0); ++i)
        run.press("B");
    run.wait(40);
    capture(run, "bag-tossed");
    close(run);
    std::puts("PASS: original bag scroll, Select swap, Use, Toss, quantities and return");
    return 0;
}
inline int shop(GBContext *ctx, bool fp, int count) {
    QaWalk run{ctx};
    run.wait(30);
    run.require(run.read(pallet::Map) == 42 && run.read(pallet::X) == 3 && run.read(pallet::Y) == 7,
                "Viridian mart fixture after parcel quest");
    inventory(ctx, count);
    camera(run, fp);
    qa_frame_observer = observe_menu_frame;
    run.move(42, 3, 5, "U");
    run.move(42, 2, 5, "L");
    run.press("L", 4);
    run.press("A");
    run.wait(180);
    capture(run, "mart-main");
    pause_and_load(run);
    if (!count) {
        run.press("D");
        run.press("A");
        run.wait(180);
        capture(run, "mart-sell-empty");
        for (int i = 0; i < 20 && !battle::live_return(ctx, 0x69cd, 0x3010); ++i) {
            run.press("B");
            run.wait(20);
        }
        run.require(battle::live_return(ctx, 0x69cd, 0x3010),
                    "empty sale returns to original main menu");
    }
    if (count == 20) {
        // Private long stock, within the original 16-byte buffer: count,
        // fourteen IDs and FF. Names/prices still come entirely from the ROM.
        ctx->wram[0xf7a] = 14;
        for (int i = 0; i < 14; ++i)
            ctx->wram[0xf7b + i] = ctx->wram[0x131d + i * 2];
        ctx->wram[0xf89] = 255;
    }
    run.press("A");
    run.wait(60);
    capture(run, "mart-buy-first");
    if (count == 20) {
        for (int i = 0; i < 14; ++i)
            run.press("D");
        run.require(run.read(0xcc26) + run.read(0xcc36) == 14,
                    "long original shop stock reaches Cancel");
        capture(run, "mart-buy-last");
        for (int i = 0; i < 14; ++i)
            run.press("U");
    }
    run.press("A");
    run.wait(40);
    run.press("U");
    run.require(run.read(0xcf95) == 2, "buy quantity uses original Up input");
    capture(run, "mart-buy-quantity");
    run.press("A");
    run.wait(200);
    for (int i = 0; i < 8 && battle::tile(ctx, 14, 7) != 0x79; ++i)
        run.press("A");
    capture(run, "mart-buy-confirm");
    int before = count ? 10 : 0;
    for (int i = 0; i < 12 && run.read(0xd31e) != before + 2; ++i) {
        run.press("A");
        run.wait(40);
    }
    run.require(run.read(0xd31d) == 4 && run.read(0xd31e) == before + 2,
                "original buy adds precisely two Poke Balls");
    for (int i = 0; i < 12 && !battle::live_return(ctx, 0x6ae0, 0x2ae0); ++i)
        run.press("B");
    run.press("B");
    run.wait(80);
    run.press("D");
    run.press("A");
    run.wait(80);
    capture(run, "mart-sell-first");
    run.require(battle::live_return(ctx, 0x6a30, 0x2ae0), "original Sell list");
    if (count == 20) {
        for (int i = 0; i < count; ++i)
            run.press("D");
        capture(run, "mart-sell-last");
        run.press("U");
        run.press("U"); // Original Town Map at index 18 is a key item.
        run.press("A");
        run.wait(180);
        capture(run, "mart-unsellable");
        for (int i = 0; i < 20 && !battle::live_return(ctx, 0x69cd, 0x3010); ++i) {
            run.press("B");
            run.wait(20);
        }
        run.require(battle::live_return(ctx, 0x69cd, 0x3010), "key item sale returns to main");
        run.press("D");
        run.press("A");
        run.wait(80);
        run.require(battle::live_return(ctx, 0x6a30, 0x2ae0) && run.read(0xcc36) == 0,
                    "original sale list resets after an unsellable key item");
    }
    run.press("A");
    run.wait(40);
    run.press("U");
    capture(run, "mart-sell-quantity");
    run.press("A");
    run.wait(200);
    for (int i = 0; i < 8 && battle::tile(ctx, 14, 7) != 0x79; ++i)
        run.press("A");
    capture(run, "mart-sell-confirm");
    for (int i = 0; i < 12 && (count ? run.read(0xd31e) != before : run.read(0xd31c) != 0); ++i) {
        run.press("A");
        run.wait(40);
    }
    if (count)
        run.require(run.read(0xd31e) == before, "original sale removes precisely two Poke Balls");
    else
        run.require(run.read(0xd31c) == 0, "original sale empties the bag");
    close(run);
    std::puts("PASS: original mart buy/sell, long stock and bag, quantities, prices and return");
    return 0;
}
inline int battle_bag(GBContext *ctx, bool fp) {
    QaWalk run{ctx};
    run.wait(30);
    run.require(battle_menu_visible(ctx) && battle::normal(ctx), "original ready battle");
    inventory(ctx, 20);
    camera(run, fp);
    run.press("D");
    run.press("L");
    run.press("A");
    run.wait(60);
    capture(run, "bag-battle-first");
    for (int i = 0; i < 20; ++i)
        run.press("D");
    run.require(run.read(0xcc26) + run.read(0xcc36) == 20,
                "battle item list reaches Cancel through original scroll");
    capture(run, "bag-battle-last");
    for (int i = 0; i < 20; ++i)
        run.press("U");
    run.press("D");
    run.press("A"); // Potion opens the original party target selector.
    run.wait(100);
    // This page belongs to the A2 party compositor, not the inventory list.
    std::string label = "logs/bag-battle-target";
    capture_surface((label + ".ppm").c_str());
    run.require(gb_context_save_state_file(ctx, (label + ".state").c_str()),
                "private original item target state");
    if (pallet3d_menu_style() == ui_preferences::Style::Integrated)
        run.require(pallet3d_pokemon_menu().active && !pallet3d_pokemon_menu().fallback,
                    "original Potion target remains an integrated party page");
    run.press("B");
    run.wait(80);
    capture(run, "bag-battle-return");
    run.press("B");
    run.wait(60);
    run.require(battle_menu_visible(ctx) && battle::normal(ctx),
                "cancelled item targeting preserves original battle turn");
    run.require(run.read(0xd320) == 99, "cancelled Potion target consumes no item");
    std::puts("PASS: original battle bag, long scroll, Potion target and cancel without a turn");
    return 0;
}
} // namespace item_qa
