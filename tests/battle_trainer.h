#pragma once
inline bool battle_has_word(const GBContext *ctx, const char *word) {
    for (int y = 0; y < 18; y++)
        for (int x = 0; x < 20 - int(std::strlen(word)); x++) {
            bool match = true;
            for (int i = 0; word[i]; i++)
                match &= battle::tile(ctx, x + i, y) == word[i] - 'A' + 0x80;
            if (match)
                return true;
        }
    return false;
}
inline int trainer_prepare(GBContext *ctx) {
    QaWalk run{ctx};
    run.wait(30);
    run.require(run.read(pallet::Map) == 12 && run.read(pallet::X) == 14,
                "captured-party Route 1 fixture");
    auto move = [&](int map, int x, int y, const char *direction) {
        run.input(direction);
        for (int i = 0; i < 1800; i++) {
            run.tick();
            if (run.read(pallet::Battle)) {
                for (int f = 0; f < 6000 && run.read(pallet::Battle); f++) {
                    if (f % 20 == 0)
                        run.input("A");
                    if (f % 20 == 6)
                        run.input(nullptr);
                    run.tick();
                }
                run.require(!run.read(pallet::Battle) && battle::word(ctx, 0xd16b) > 0,
                            "survive route encounter");
                run.input(direction);
            }
            if (run.read(pallet::Map) == map && run.read(pallet::X) == x &&
                run.read(pallet::Y) == y && !run.read(pallet::Walk)) {
                run.input(nullptr);
                return;
            }
        }
        capture_surface("logs/trainer-prepare-stuck.ppm");
        run.require(false, "trainer preparation waypoint");
    };
    move(12, 14, 4, "U");
    move(12, 10, 4, "L");
    move(1, 20, 30, "U");
    move(1, 19, 30, "L");
    move(1, 19, 26, "U");
    move(1, 23, 26, "R");
    move(41, 3, 7, "U");
    run.wait(50);
    move(41, 3, 3, "U");
    run.press("U", 8);
    run.press("A", 8);
    for (int i = 0; i < 2500; i++) {
        if (i % 20 == 0)
            run.input("A");
        if (i % 20 == 6)
            run.input(nullptr);
        run.tick();
        if (i > 150 && pallet::view(ctx) == pallet::View::Overworld &&
            battle::word(ctx, 0xd16b) == battle::word(ctx, 0xd18c))
            break;
    }
    run.input(nullptr);
    move(1, 23, 26, "D");
    run.wait(50);
    move(1, 19, 26, "L");
    move(1, 19, 21, "U");
    move(1, 8, 21, "L");
    move(1, 8, 20, "U");
    move(1, 6, 20, "L");
    move(1, 6, 15, "U");
    move(33, 39, 7, "L");
    move(33, 36, 7, "L");
    move(33, 36, 12, "D");
    move(33, 33, 12, "L");
    move(33, 33, 8, "U");
    move(33, 31, 8, "L");
    move(33, 31, 5, "U");
    move(33, 30, 5, "L");
    run.wait(40);
    run.require(gb_context_save_state_file(ctx, "logs/route22-trainer.state"), "trainer fixture");
    capture_surface("logs/route22-trainer.ppm");
    std::puts("PASS: real Route 1, healing and Route 22 trainer approach");
    return 0;
}
inline int battle_trainer(GBContext *ctx) {
    QaWalk run{ctx};
    run.wait(30);
    run.require(run.read(pallet::Map) == 33 && run.read(pallet::X) == 30 &&
                    run.read(pallet::Y) == 5,
                "Route 22 rival fixture");
    auto capture = [&](const char *name) {
        std::string path = std::string("logs/") + name;
        capture_surface((path + ".ppm").c_str());
        run.require(gb_context_save_state_file(ctx, (path + ".state").c_str()),
                    "private trainer fixture");
    };
    run.move(33, 29, 5, "L");
    run.input(nullptr);
    int frames = 0, previous_enemy = 0, replacements = 0;
    uint64_t last_player_image = 0;
    int last_player_species = 0;
    bool begun = false, intro = false, forced = false, saw_faint = false, saw_loss = false;
    bool toured_menus = false;
    for (int i = 0; i < 15000; i++) {
        if (i % 20 == 0)
            run.input(battle_has_word(ctx, "YES") && battle_has_word(ctx, "NO") ? "B" : "A");
        if (i % 20 == 6)
            run.input(nullptr);
        run.tick();
        if (run.read(pallet::Battle) == 255) {
            saw_loss = true;
            run.expected_respawn = 1;
        }
        if (begun && battle::word(ctx, 0xd014) == 0)
            saw_faint = true;
        // Route 1 can yield Pidgey or Rattata. Recognize the actual second
        // party member rather than coupling the fainting test to encounter RNG.
        auto reserve_name = battle::text(ctx, 0xd2bf);
        if (!forced && battle::word(ctx, 0xd014) == 0 && !reserve_name.empty() &&
            battle_has_word(ctx, reserve_name.c_str()) &&
            pallet::view(ctx) == pallet::View::Unsupported) {
            run.input(nullptr);
            run.wait(45);
            capture("trainer-player-fainted");
            run.press("D");
            run.press("A");
            run.wait(20);
            run.press("A");
            forced = true;
        }
        if (run.read(pallet::Battle) == 2) {
            begun = true;
            if (!intro && battle::trainer_intro(ctx) && pallet3d_battle().enemy_alpha > .8f) {
                capture("trainer-intro");
                intro = true;
            }
        }
        auto info = pallet3d_battle();
        // Opening now has its own arena before either fighter's HUD exists.
        // Exact fighter comparisons apply once the original battle is ready.
        if (info.active && battle::ready(ctx) && !info.full_overlay && !info.trainer_class &&
            !battle::animation_running(ctx)) {
            ++frames;
            run.require(info.enemy_image == battle::fingerprint(battle::portrait(
                                                ctx, battle::Enemy, info.enemy_species)),
                        "enemy portrait tracks trainer replacement");
            if (battle::rectangle(ctx, battle::Player, true)) {
                run.require(info.player_image == battle::fingerprint(battle::portrait(
                                                     ctx, battle::Player, info.player_species)),
                            "player portrait tracks current fighter");
                last_player_image = info.player_image;
                last_player_species = info.player_species;
            } else {
                run.require(info.integrated_menu &&
                                info.menu_kind == int(battle_menu::Kind::Moves) &&
                                last_player_image && info.player_image == last_player_image &&
                                info.player_species == last_player_species,
                            "PP/type overlay retains the verified current player portrait");
            }
            if (previous_enemy != info.enemy_species && info.enemy_alpha > .8f) {
                if (previous_enemy)
                    ++replacements;
                previous_enemy = info.enemy_species;
                capture(replacements ? "trainer-second-mon" : "trainer-first-mon");
            }
        }
        if (!toured_menus && pallet3d_menu_style() == ui_preferences::Style::Integrated &&
            battle_menu_visible(ctx) && info.integrated_menu && info.player_alpha > .8f) {
            run.input(nullptr);
            run.wait(12);
            capture("trainer-fight");
            run.press("A");
            run.wait(35);
            run.require(pallet3d_battle().integrated_menu &&
                            pallet3d_battle().menu_kind == int(battle_menu::Kind::Moves),
                        "trainer move list and PP/type integrated");
            capture("trainer-moves");
            run.press("B");
            run.wait(20);
            run.press("D");
            run.press("A");
            run.wait(35);
            run.require(!battle_menu_visible(ctx) && pallet3d_battle().full_overlay,
                        "trainer bag retains full LCD");
            capture("trainer-bag");
            run.press("B");
            run.wait(35);
            run.require(battle_menu_visible(ctx), "trainer bag returns to FIGHT");
            run.press("U");
            run.press("R");
            run.press("A");
            run.wait(35);
            run.require(!battle_menu_visible(ctx) && pallet3d_battle().full_overlay,
                        "trainer party retains full LCD");
            capture("trainer-party");
            run.press("B");
            run.wait(35);
            run.require(battle_menu_visible(ctx), "trainer party returns to FIGHT");
            run.press("D");
            run.press("A");
            run.wait(35);
            run.require(run.read(pallet::Battle) == 2 && !battle_menu_visible(ctx),
                        "original engine rejects fleeing a trainer");
            capture("trainer-cannot-run");
            for (int f = 0; f < 1200 && !battle_menu_visible(ctx); ++f) {
                if (f % 30 == 0)
                    run.input("A");
                if (f % 30 == 6)
                    run.input(nullptr);
                run.tick();
            }
            run.input(nullptr);
            run.require(battle_menu_visible(ctx) && run.read(pallet::Battle) == 2,
                        "trainer continues after rejected escape");
            run.press("L");
            run.press("U");
            toured_menus = true;
        }
        if (begun && !run.read(pallet::Battle) && pallet::view(ctx) == pallet::View::Overworld &&
            i > 1000)
            break;
    }
    // Winning clears IsInBattle before the rival's closing dialogue/script.
    // Finish that original script and prove a stable return to the world.
    run.input(nullptr);
    int stable_world = 0;
    for (int i = 0; i < 2500 && stable_world < 90; i++) {
        bool world = pallet::view(ctx) == pallet::View::Overworld && !run.read(pallet::Battle);
        if (world)
            run.input(nullptr);
        else if (i % 20 == 0)
            run.input("A");
        else if (i % 20 == 6)
            run.input(nullptr);
        run.tick();
        stable_world = world ? stable_world + 1 : 0;
    }
    run.input(nullptr);
    capture("trainer-result");
    std::fprintf(stderr,
                 "[TRAINER] scene_frames=%d replacements=%d faint=%d loss=%d battle=%d "
                 "partyHP=%d/%d map=%d\n",
                 frames, replacements, saw_faint, saw_loss, run.read(pallet::Battle),
                 battle::word(ctx, 0xd16b), battle::word(ctx, 0xd197), run.read(pallet::Map));
    run.require(begun && !run.read(pallet::Battle) && frames > 100,
                "trainer battle finishes with 3D scene");
    run.require(replacements >= 1, "trainer sends a second Pokemon");
    run.require(intro, "trainer portrait shown before first enemy Pokemon");
    if (pallet3d_menu_style() == ui_preferences::Style::Integrated)
        run.require(toured_menus, "trainer fight/moves/bag/party/run inputs exercised");
    run.require(stable_world == 90 && pallet3d_active(),
                "trainer closing dialogue returns to a stable 3D world");
    if (saw_loss)
        run.require(run.read(pallet::Map) == 1 && run.read(pallet::X) == 23 &&
                        run.read(pallet::Y) == 26 && battle::word(ctx, 0xd16b) > 0 &&
                        battle::word(ctx, 0xd197) > 0,
                    "blackout returns to healing location and restores the party");
    std::puts("PASS: real trainer battle and enemy replacement, exact VRAM portraits, GL and "
              "read-only memory");
    return 0;
}
