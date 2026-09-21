#pragma once
#include "menu_layout.h"
#include "pc_storage.h"

inline bool start_visible(const GBContext *ctx) {
    auto layout = menu_layout::classify(ctx->wram + 0x3a0);
    return layout.kind == menu_layout::Kind::Partial && layout.count == 1 &&
           layout.regions[0].x == 10;
}

inline void observe_menu_frame(GBContext *c, int frame) {
    if (!pallet3d_active() || !pallet3d_covers_frame(c)) {
        std::fprintf(stderr, "[MENU] FAIL: 2D gap frame=%d view=%d bgp=%02x font=%d live=%d\n",
                     frame, int(pallet::view(c)), c->io[0x47], pallet::read(c, pallet::Font),
                     menu_state::running(c));
        std::exit(42);
    }
    auto storage = pallet3d_storage();
    if (pallet3d_pc().mode == int(pc_state::Mode::Bill) && pc_storage::read(c).valid) {
        if (!storage.active || storage.box != (pallet::read(c, 0xd59f) & 0x7f) ||
            storage.counts[storage.box] != pallet::read(c, 0xda7f) || storage.cached > 32) {
            std::fprintf(stderr, "[PC] FAIL stale shelf frame=%d\n", frame);
            std::exit(49);
        }
        for (int i = 0; i < storage.counts[storage.box]; i++)
            if (storage.species[i] != pallet::read(c, uint16_t(0xda80 + i))) {
                std::fprintf(stderr, "[PC] FAIL stale box portrait frame=%d index=%d\n", frame, i);
                std::exit(49);
            }
        for (int i = 0; i < pallet::read(c, 0xd162); i++)
            if (storage.species[20 + i] != pallet::read(c, uint16_t(0xd163 + i))) {
                std::fprintf(stderr, "[PC] FAIL stale party portrait frame=%d index=%d\n", frame,
                             i);
                std::exit(49);
            }
    }
}

// Extra integrated input coverage leaves the classic reference journey intact.
inline void exercise_integrated_cursor(QaWalk &run) {
    if (pallet3d_menu_style() != ui_preferences::Style::Integrated)
        return;
    int initial = run.read(0xcc26);
    run.press("D");
    run.require(run.read(0xcc26) != initial, "original cursor moves down");
    run.press("U");
    run.require(run.read(0xcc26) == initial, "original cursor returns to its entry");
}

inline void verify_menu_overlay(QaWalk &run, const char *label, int expected = -1) {
    auto *ctx = run.ctx;
    auto info = pallet3d_menu();
    auto pc = pallet3d_pc();
    auto layout = menu_layout::classify(ctx->wram + 0x3a0);
    std::string path = std::string("logs/") + label;
    capture_surface((path + ".ppm").c_str());
    run.require(gb_context_save_state_file(ctx, (path + ".state").c_str()), "private menu fixture");
    FILE *tiles = std::fopen((path + ".tiles").c_str(), "wb");
    run.require(tiles, "private tilemap export");
    std::fwrite(ctx->wram + 0x3a0, 1, 360, tiles);
    std::fclose(tiles);
    std::fprintf(
        stderr,
        "[MENU] %s view=%d kind=%d regions=%d full=%d blur=%d live=%d sp=%04x cursor=%d max=%d\n",
        label, int(pallet::view(ctx)), int(layout.kind), info.regions, info.full, info.blurred,
        menu_state::running(ctx), ctx->sp, run.read(0xcc26), run.read(0xcc28));
    if (info.full && !info.blurred && !pc.monitor) {
        std::fprintf(stderr, "[MENU] unexpected fade=%d bgp=%02x lcdc=%02x live warp=%d\n",
                     pallet3d_warp_overlay(), ctx->io[0x47], ctx->io[0x40], fade::warp(ctx));
        for (int a = ctx->sp; a < 0xdfff; a += 2)
            std::fprintf(stderr, " %04x:%04x", a, run.read(a) | (run.read(a + 1) << 8));
        std::fputc('\n', stderr);
    }
    run.require((info.active || pc.active) && pallet3d_active() && pallet3d_covers_frame(ctx),
                "menu retains a 3D background");
    if (expected >= 0)
        run.require(int(layout.kind) == expected, "expected original menu layout");
    run.require(!info.full || info.blurred || (pc.active && pc.monitor && pc.progress == 1),
                "full screen has blur or is on the focused PC monitor");
    run.require(pallet3d_input_mask() == 255, "menus neutralize relative movement");
    auto camera = pallet3d_world_frame();
    auto cache = pallet3d_stats();
    ReadOnlyMemory memory{ctx};
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int w = viewport[2], h = viewport[3];
    std::vector<uint8_t> before(size_t(w) * h * 4), after(before.size());
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, before.data());
    auto uploads = pallet3d_overlay_stats();
    auto storage = pallet3d_storage();
    for (int i = 0; i < 3; i++)
        gb_platform_render_frame(gb_get_framebuffer(ctx));
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, after.data());
    run.require(memory.unchanged(ctx) && glGetError() == GL_NO_ERROR,
                "read-only menu composition and GL");
    run.require(before == after && camera.camera == pallet3d_world_frame().camera,
                "menu background and camera are frozen");
    run.require(cache.mesh_builds == pallet3d_stats().mesh_builds &&
                    uploads.uploads == pallet3d_overlay_stats().uploads,
                "no mesh rebuild or unchanged LCD upload");
    if (storage.active)
        run.require(storage.rebuilds == pallet3d_storage().rebuilds &&
                        storage.uploads == pallet3d_storage().uploads,
                    "unchanged box/party bytes reuse shelf geometry and portraits");
    if (pallet3d_menu_style() == ui_preferences::Style::Integrated && !info.full && !pc.active) {
        run.require(ui_style_qa::enabled, "integrated capture requires per-frame glyph oracle");
        ui_style_qa::observe(ctx, w, h, false);
        std::fprintf(stderr, "[MENU] %s integrated glyphs verified against ROM and wTileMap\n",
                     label);
        return;
    }
    int scale = info.full ? std::max(1, int(std::min(w * .75f / 160, h * .75f / 144)))
                          : std::max(1, std::min(w / 160, h / 144));
    int left = (w - 160 * scale) / 2, top = info.full ? (h - 144 * scale) / 2 : h - 144 * scale;
    int checked = 0, differences = 0;
    for (int y = 0; y < 144; y++)
        for (int x = 0; x < 160; x++) {
            bool ui = info.full;
            for (size_t i = 0; i < layout.count; i++)
                ui |= menu_layout::contains(layout.regions[i], x / 8, y / 8);
            if (!ui)
                continue;
            int xx = left + x * scale + scale / 2, yy = top + y * scale + scale / 2;
            if (pc.active && pc.monitor) {
                float u = (x + .5f) / 160, v = (y + .5f) / 144;
                xx = int(pc.screen[0] * (1 - u) * (1 - v) + pc.screen[2] * u * (1 - v) +
                         pc.screen[4] * u * v + pc.screen[6] * (1 - u) * v);
                yy = int(pc.screen[1] * (1 - u) * (1 - v) + pc.screen[3] * u * (1 - v) +
                         pc.screen[5] * u * v + pc.screen[7] * (1 - u) * v);
                run.require(xx >= 0 && xx < w && yy >= 0 && yy < h, "complete PC screen visible");
            }
            auto *pixel = &after[((h - 1 - yy) * w + xx) * 4];
            auto original = gb_get_framebuffer(ctx)[y * 160 + x];
            ++checked;
            bool different = pixel[0] != (original >> 16 & 255) ||
                             pixel[1] != (original >> 8 & 255) || pixel[2] != (original & 255);
            if (different && differences < 8)
                std::fprintf(stderr,
                             "[MENU] mismatch LCD=%d,%d screen=%d,%d actual=%02x%02x%02x "
                             "expected=%06x monitor=%d\n",
                             x, y, xx, yy, pixel[0], pixel[1], pixel[2],
                             unsigned(original & 0xffffff), pc.monitor);
            differences += different;
        }
    std::fprintf(stderr, "[MENU] %s original pixels=%d differences=%d\n", label, checked,
                 differences);
    run.require(checked > 0 && differences == 0,
                "every composed LCD pixel retains its original color");
    if (info.full)
        run.require(left >= 16 && top >= 16,
                    "full menu leaves the background visible around its frame");
}

inline int menus_journey(GBContext *ctx, bool fp) {
    QaWalk run{ctx};
    run.wait(30);
    run.require(pallet::view(ctx) == pallet::View::Overworld && run.read(0xd162) >= 2,
                "world fixture with two real party members");
    if (run.read(pallet::Map) == 37) {
        run.move(37, 2, 6, "U");
        run.move(0, 5, 6, "D");
        run.wait(45);
        run.move(0, 5, 8, "D");
    }
    if (fp) {
        SDL_Event e{};
        e.type = SDL_KEYDOWN;
        e.key.keysym.scancode = SDL_SCANCODE_F3;
        pallet3d_event(&e, false);
        run.wait(30);
    }
    capture_surface("logs/menus-world-before.ppm");
    qa_frame_observer = observe_menu_frame;
    auto capture = [&](const char *label, int kind = -1) { verify_menu_overlay(run, label, kind); };
    auto start = [&]() {
        if (pallet::view(ctx) == pallet::View::Overworld && !menu_state::running(ctx))
            run.press("S");
        for (int i = 0; i < 12 && !start_visible(ctx); i++)
            run.press("B");
        run.wait(20);
        run.require(start_visible(ctx), "return to Start menu");
    };
    auto choose = [&](int index) {
        start();
        for (int i = 0; i < 10 && run.read(0xcc26) > 0; i++)
            run.press("U");
        run.require(run.read(0xcc26) == 0, "Start cursor normalized");
        for (int i = 0; i < index; i++)
            run.press("D");
        run.press("A");
        run.wait(60);
    };
    start();
    capture("start", 1);
    run.require(menu_layout::classify(ctx->wram + 0x3a0).regions[0].h == 16,
                "fixture has the Pokedex");
    for (int i = 0; i < 7; i++)
        run.press("D"); // Exercise wraparound and all entries.
    choose(1);
    capture("party", 2);
    int first = run.read(0xd163), second = run.read(0xd164);
    run.press("A");
    run.wait(20);
    capture("party-actions", 2);
    int stats = run.read(0xcc28) - 2;
    run.require(stats >= 0 && stats < 5, "Stats index after field moves");
    for (int i = 0; i < stats; i++)
        run.press("D");
    run.press("A");
    run.wait(80);
    capture("stats", 2);
    run.press("A");
    run.wait(60);
    capture("stats-moves", 2);
    run.press("B");
    run.wait(60);
    capture("party-return", 2);
    run.press("A");
    run.wait(20);
    int change = run.read(0xcc28) - 1;
    for (int i = 0; i < change; i++)
        run.press("D");
    run.press("A");
    run.press("D");
    run.press("A");
    run.wait(30);
    run.require(run.read(0xd163) == second && run.read(0xd164) == first,
                "original Switch menu swaps the party order");
    capture("party-swapped", 2);
    choose(2);
    capture("bag", 2);
    choose(3);
    capture("trainer-card", 2);
    choose(5);
    capture("options", 2);
    choose(0);
    run.require(pallet3d_dex().active && pallet3d_dex().list,
                "Pokedex hands off to its own device");
    capture_surface("logs/pokedex.ppm");
    run.require(gb_context_save_state_file(ctx, "logs/pokedex.state"),
                "private Pokedex list fixture");
    FILE *dex_tiles = std::fopen("logs/pokedex.tiles", "wb");
    run.require(dex_tiles, "private original Pokedex tilemap");
    std::fwrite(ctx->wram + 0x3a0, 1, 360, dex_tiles);
    std::fclose(dex_tiles);
    choose(4);
    run.wait(180);
    capture("save-question", 1);
    exercise_integrated_cursor(run);
    std::vector<uint8_t> old_save(ctx->eram, ctx->eram + ctx->eram_size);
    for (int i = 0; i < 4 && std::equal(old_save.begin(), old_save.end(), ctx->eram); i++) {
        run.press("A");
        run.wait(180);
    }
    capture("save-result", 1);
    run.require(!std::equal(old_save.begin(), old_save.end(), ctx->eram),
                "original Save writes the private SRAM");
    FILE *battery = std::fopen("logs/menu-battery.sav", "wb");
    run.require(battery, "private battery fixture");
    std::fwrite(ctx->eram, 1, ctx->eram_size, battery);
    std::fclose(battery);
    for (int i = 0;
         i < 12 && (pallet::view(ctx) != pallet::View::Overworld || menu_state::running(ctx)); i++)
        run.press("B");
    run.wait(30);
    run.require(pallet::view(ctx) == pallet::View::Overworld && !pallet3d_menu().active,
                "menus restore world and HUD");
    run.require(pallet3d_firstperson() == fp, "menus preserve camera preference");
    qa_frame_observer = nullptr;
    capture_surface("logs/menus-world-return.ppm");
    run.require(gb_context_save_state_file(ctx, "logs/menus-world-return.state"),
                "private world return fixture");
    std::puts("PASS: Start entries, party stats/order, bag, card, options, Pokedex and original "
              "save; retained scene, exact LCD, GL and read-only rendering");
    return 0;
}

inline int menus_center(GBContext *ctx, bool fp) {
    QaWalk run{ctx};
    run.wait(30);
    run.require(run.read(pallet::Map) == 41 && run.read(pallet::X) == 3 &&
                    run.read(pallet::Y) == 7 && run.read(0xd162) == 2,
                "Viridian center entrance with two real party members");
    if (fp) {
        SDL_Event e{};
        e.type = SDL_KEYDOWN;
        e.key.keysym.scancode = SDL_SCANCODE_F3;
        pallet3d_event(&e, false);
        run.wait(30);
    }
    qa_frame_observer = observe_menu_frame;
    run.move(41, 3, 3, "U");
    run.press("U", 4);
    run.press("A");
    run.wait(240);
    for (int i = 0; i < 8 && battle::tile(ctx, 11, 6) != 0x79; i++) {
        run.press("A");
        run.wait(80);
    }
    run.require(battle::tile(ctx, 11, 6) == 0x79, "original Heal/Cancel choice reached");
    verify_menu_overlay(run, "center-question", 1);
    exercise_integrated_cursor(run);
    for (int i = 0; i < 45; i++) {
        run.press("A");
        run.wait(40);
        if (pallet::view(ctx) == pallet::View::Overworld && !menu_state::running(ctx))
            break;
    }
    run.require(pallet::view(ctx) == pallet::View::Overworld &&
                    battle::word(ctx, 0xd16b) == battle::word(ctx, 0xd18c),
                "original nurse heals and releases the menu");
    capture_surface("logs/center-healed.ppm");
    run.move(41, 3, 5, "D");
    run.move(41, 13, 5, "R");
    run.move(41, 13, 4, "U");
    run.press("U", 4);
    run.press("A");
    run.wait(200);
    verify_menu_overlay(run, "pc-start");
    auto wait_menu = [&](int maximum) {
        for (int i = 0; i < 10 && run.read(0xcc28) != maximum; i++) {
            run.press("A");
            run.wait(100);
        }
        run.require(run.read(0xcc28) == maximum, "original PC menu reached");
    };
    wait_menu(3);
    verify_menu_overlay(run, "pc-main", 2);
    run.press("A");
    run.wait(160);
    wait_menu(5);
    verify_menu_overlay(run, "pc-bill", 2);
    int first = run.read(0xd163), second = run.read(0xd164);
    run.press("D");
    run.press("A");
    run.wait(60);
    verify_menu_overlay(run, "pc-deposit-list", 2);
    run.press("D");
    run.press("A");
    run.wait(60);
    verify_menu_overlay(run, "pc-deposit-action", 2);
    run.press("A");
    run.wait(200);
    run.require(run.read(0xd162) == 1 && run.read(0xd163) == first,
                "original PC deposits the second Pokemon");
    wait_menu(5);
    verify_menu_overlay(run, "pc-deposited", 2);
    run.press("U");
    run.press("A");
    run.wait(60);
    verify_menu_overlay(run, "pc-withdraw-list", 2);
    run.press("A");
    run.wait(60);
    verify_menu_overlay(run, "pc-withdraw-action", 2);
    run.press("A");
    run.wait(200);
    run.require(run.read(0xd162) == 2 && run.read(0xd164) == second,
                "original PC withdraws the same Pokemon");
    for (int i = 0;
         i < 15 && (pallet::view(ctx) != pallet::View::Overworld || menu_state::running(ctx));
         i++) {
        run.press("B");
        run.wait(40);
    }
    run.wait(30);
    run.require(pallet::view(ctx) == pallet::View::Overworld && !pallet3d_menu().active,
                "PC restores world and HUD");
    qa_frame_observer = nullptr;
    capture_surface("logs/pc-world-return.ppm");
    std::puts("PASS: original healing, PC deposit/withdraw, retained scene and exact LCD in "
              "selected camera");
    return 0;
}

inline int menus_shop(GBContext *ctx, bool fp) {
    QaWalk run{ctx};
    run.wait(30);
    run.require(run.read(pallet::Map) == 42 && run.read(pallet::X) == 3 && run.read(pallet::Y) == 7,
                "Viridian shop entrance after parcel quest");
    if (fp) {
        SDL_Event e{};
        e.type = SDL_KEYDOWN;
        e.key.keysym.scancode = SDL_SCANCODE_F3;
        pallet3d_event(&e, false);
        run.wait(30);
    }
    auto balls = [&]() {
        for (int i = 0; i < run.read(0xd31c) && i < 20; i++)
            if (run.read(0xd31d + i * 2) == 4)
                return run.read(0xd31e + i * 2);
        return 0;
    };
    qa_frame_observer = observe_menu_frame;
    run.move(42, 3, 5, "U");
    run.move(42, 2, 5, "L");
    run.press("L", 4);
    int before = balls();
    run.press("A");
    run.wait(180);
    verify_menu_overlay(run, "shop-buy", 1);
    exercise_integrated_cursor(run);
    run.press("A");
    run.wait(60);
    verify_menu_overlay(run, "shop-stock", 1);
    exercise_integrated_cursor(run);
    run.press("A");
    run.wait(60);
    verify_menu_overlay(run, "shop-quantity", 1);
    run.press("A");
    run.wait(400);
    verify_menu_overlay(run, "shop-confirm", 1);
    for (int i = 0; i < 6 && balls() == before; i++) {
        run.press("A");
        run.wait(60);
    }
    run.require(balls() == before + 1, "original shop buys one Poke Ball");
    for (int i = 0;
         i < 15 && (pallet::view(ctx) != pallet::View::Overworld || menu_state::running(ctx));
         i++) {
        run.press("B");
        run.wait(40);
    }
    run.wait(30);
    run.require(pallet::view(ctx) == pallet::View::Overworld && !pallet3d_menu().active,
                "shop restores world and HUD");
    qa_frame_observer = nullptr;
    capture_surface("logs/shop-world-return.ppm");
    std::puts("PASS: original shop purchase, partial windows, retained scene and exact LCD in "
              "selected camera");
    return 0;
}

inline int menus_name(GBContext *ctx, bool fp) {
    QaWalk run{ctx};
    run.wait(30);
    run.require(run.read(pallet::Map) == 229 && run.read(0xd162) == 2,
                "Name Rater house with two real party members");
    if (fp) {
        SDL_Event e{};
        e.type = SDL_KEYDOWN;
        e.key.keysym.scancode = SDL_SCANCODE_F3;
        pallet3d_event(&e, false);
        run.wait(30);
    }
    qa_frame_observer = observe_menu_frame;
    run.move(229, 2, 6, "U");
    run.move(229, 6, 6, "R");
    run.move(229, 6, 3, "U");
    run.press("L", 4);
    run.press("A");
    auto until = [&](auto predicate, const char *message) {
        for (int i = 0; i < 2400 && !predicate(); i++) {
            if (i % 90 == 0)
                run.input("A");
            if (i % 90 == 4)
                run.input(nullptr);
            run.tick();
        }
        run.input(nullptr);
        run.require(predicate(), message);
        run.wait(30);
    };
    until([&]() { return battle::tile(ctx, 14, 7) == 0x79; }, "name rating Yes/No prompt");
    verify_menu_overlay(run, "name-question", 1);
    exercise_integrated_cursor(run);
    run.press("A");
    until(
        [&]() {
            return battle::tile(ctx, 0, 0) == 0x7f && run.read(0xcc28) == 1 &&
                   menu_layout::classify(ctx->wram + 0x3a0).kind == menu_layout::Kind::Full;
        },
        "name rating party list");
    verify_menu_overlay(run, "name-party", 2);
    run.press("D");
    run.press("A");
    until([&]() { return battle::tile(ctx, 14, 7) == 0x79; }, "rename confirmation");
    run.press("A");
    until(
        [&]() {
            return battle::tile(ctx, 2, 5) == 0x80 && battle::tile(ctx, 4, 5) == 0x81 &&
                   battle::tile(ctx, 6, 5) == 0x82;
        },
        "original naming alphabet");
    verify_menu_overlay(run, "naming-empty", 2);
    // Clear any letter accepted while advancing the preceding text, then type
    // ABC and submit with the original Start shortcut. No name bytes are set.
    for (int i = 0; i < 10; i++)
        run.press("B");
    run.press("A");
    run.press("R");
    run.press("A");
    run.press("R");
    run.press("A");
    verify_menu_overlay(run, "naming-typed", 2);
    run.press("S");
    run.wait(100);
    run.require(run.read(0xd2bf) == 0x80 && run.read(0xd2c0) == 0x81 && run.read(0xd2c1) == 0x82 &&
                    run.read(0xd2c2) == 0x50,
                "original naming screen stores ABC in second party nickname");
    for (int i = 0;
         i < 20 && (pallet::view(ctx) != pallet::View::Overworld || menu_state::running(ctx));
         i++) {
        run.press("B");
        run.wait(30);
    }
    run.require(pallet::view(ctx) == pallet::View::Overworld && !pallet3d_menu().active,
                "naming returns to house");
    qa_frame_observer = nullptr;
    capture_surface("logs/naming-world-return.ppm");
    std::puts("PASS: real Name Rater dialogue, party choice, ABC input and return; retained scene "
              "and exact LCD");
    return 0;
}

inline void verify_bottom_overlay(QaWalk &run, const char *label) {
    auto *ctx = run.ctx;
    run.require(pallet3d_dialogue_overlay() && pallet3d_active(),
                "dialogue keeps its 3D scene in both cameras");
    run.require(pallet3d_input_mask() == 255, "text has no relative movement input");
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int w = viewport[2], h = viewport[3], scale = std::max(1, std::min(w / 160, h / 144));
    std::vector<uint8_t> before(size_t(w) * h * 4), after(before.size());
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, before.data());
    ReadOnlyMemory memory{ctx};
    auto uploads = pallet3d_overlay_stats();
    for (int i = 0; i < 3; i++)
        gb_platform_render_frame(gb_get_framebuffer(ctx));
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, after.data());
    run.require(glGetError() == GL_NO_ERROR && memory.unchanged(ctx),
                "repeated composition has no GL errors or machine writes");
    run.require(uploads.uploads == pallet3d_overlay_stats().uploads &&
                    uploads.bytes == pallet3d_overlay_stats().bytes,
                "unchanged LCD regions do not trigger texture uploads");
    run.require(before == after, "camera and HUD stay fixed during the dialogue");
    if (pallet3d_menu_style() == ui_preferences::Style::Integrated) {
        run.require(ui_style_qa::enabled, "integrated dialogue requires glyph oracle");
        ui_style_qa::observe(ctx, w, h, false);
        return;
    }
    const auto *lcd = gb_get_framebuffer(ctx);
    int differences = 0, background_differences = 0;
    for (int y = 0; y < 144; y++)
        for (int x = 0; x < 160; x++) {
            int xx = (w - 160 * scale) / 2 + x * scale + scale / 2,
                yy = h - 144 * scale + y * scale + scale / 2;
            auto *p = &after[((h - 1 - yy) * w + xx) * 4];
            uint32_t c = lcd[y * 160 + x];
            bool different = p[0] != (c >> 16 & 255) || p[1] != (c >> 8 & 255) || p[2] != (c & 255);
            if (y >= 96)
                differences += different;
            else
                background_differences += different;
        }
    run.require(!differences && background_differences > 100,
                "all 7680 dialogue pixels match the LCD while the background stays 3D");
    std::fprintf(
        stderr,
        "[MENU] %s camera=%s LCD differences=%d background differences=%d unchanged uploads=%zu\n",
        label, pallet3d_firstperson() ? "first-person" : "orthographic", differences,
        background_differences, uploads.uploads);
}

inline int dialogue_journey(GBContext *ctx) {
    QaWalk run{ctx};
    run.wait(30);
    run.require(run.read(pallet::Map) == 0, "Pallet exterior dialogue fixture");
    if (run.read(pallet::Y) == 7)
        run.move(0, run.read(pallet::X), 8, "D");
    for (bool fp : {false, true}) {
        if (fp) {
            SDL_Event e{};
            e.type = SDL_KEYDOWN;
            e.key.keysym.scancode = SDL_SCANCODE_F3;
            pallet3d_event(&e, false);
            run.wait(20);
        }
        bool talked = false;
        for (int attempt = 0; attempt < 8 && !talked; attempt++) {
            // Follow the real girl NPC in Pallet, including her random walking.
            // The fixture path is the clear space south of the houses.
            for (int i = 0; i < 1500; i++) {
                pallet::Actor npc{};
                run.require(pallet::actor(ctx, 2, npc), "Pallet girl is visible");
                int dx = int(npc.x) - run.read(pallet::X), dy = int(npc.z) - run.read(pallet::Y);
                if (std::abs(dx) + std::abs(dy) == 1 && !run.read(pallet::Walk)) {
                    run.input(nullptr);
                    run.press(dx ? dx > 0 ? "R" : "L" : dy > 0 ? "D" : "U", 4);
                    break;
                }
                run.input(dx ? dx > 0 ? "R" : "L" : dy > 0 ? "D" : "U");
                run.tick();
                run.require(run.read(pallet::Map) == 0, "NPC approach stays in Pallet");
            }
            run.input(nullptr);
            run.press("A", 8);
            run.wait(100);
            talked = pallet::view(ctx) == pallet::View::Dialogue && pallet::bottom_dialogue(ctx);
        }
        run.require(talked, "original NPC interaction starts a recognized text box");
        verify_bottom_overlay(run, "Pallet NPC");
        std::string path = std::string("logs/pallet-npc-") + (fp ? "fp" : "ortho");
        capture_surface((path + ".ppm").c_str());
        run.require(gb_context_save_state_file(ctx, (path + ".state").c_str()),
                    "private NPC fixture");
        for (int i = 0; i < 20 && pallet::view(ctx) != pallet::View::Overworld; i++)
            run.press("B", 8);
        run.require(pallet::view(ctx) == pallet::View::Overworld && pallet3d_active(),
                    "NPC dialogue returns to world");
    }
    std::puts("PASS: Pallet NPC in both cameras, exact original text, stable background, cached "
              "uploads, GL and read-only rendering");
    return 0;
}
