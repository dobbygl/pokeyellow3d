#pragma once
inline int interior_journey(GBContext *ctx, bool fp = false) {
    QaWalk run{ctx};
    run.wait(30);
    if (fp) {
        SDL_Event e{};
        e.type = SDL_KEYDOWN;
        e.key.keysym.scancode = SDL_SCANCODE_F3;
        pallet3d_event(&e, false);
    }
    auto capture = [&](const char *name) {
        if (const char *dir = std::getenv("INTERIOR_CAPTURE_DIR"))
            capture_surface((std::string(dir) + "/" + name + (fp ? "-fp.ppm" : ".ppm")).c_str());
    };
    auto room = [&](int id, const char *name) {
        run.input(nullptr);
        for (int i = 0; i < 240; i++) {
            run.tick();
            if (i >= 40 && pallet::view(ctx) == pallet::View::Overworld)
                break;
        }
        std::fprintf(stderr,
                     "[INTERIOR] map=%d xy=%d,%d view=%d tileset=%d sprites=%d sprite0=%d bgp=%02x "
                     "lcdc=%02x live=%d\n",
                     run.read(pallet::Map), run.read(pallet::X), run.read(pallet::Y),
                     int(pallet::view(ctx)), run.read(pallet::Tileset),
                     run.read(pallet::UpdateSprites), run.read(pallet::Sprite1), ctx->io[0x47],
                     ctx->io[0x40], pallet::valid_live_map(ctx, *pallet::scene(id)));
        run.require(run.read(pallet::Map) == id && pallet3d_active(),
                    "expected interior is presented in 3D");
        run.require(pallet3d_stats().resident_maps == 1, "interior has one resident mesh");
        run.require(pallet::scene(id)->interior && pallet::scene(id)->component == id,
                    "isolated interior coordinates");
        run.require(pallet3d_firstperson() == fp, "interior preserves camera preference");
        capture(name);
        if (const char *dir = std::getenv("INTERIOR_CAPTURE_DIR"))
            run.require(
                gb_context_save_state_file(ctx, (std::string(dir) + "/" + name + ".state").c_str()),
                "save private interior fixture");
    };
    run.require(run.read(pallet::Map) == 0 && run.read(pallet::X) == 9 && run.read(pallet::Y) == 8,
                "Pallet house fixture");
    run.move(0, 5, 8, "L");
    run.move(37, 2, 7, "U");
    room(37, "reds-house-1f");
    run.move(37, 2, 6, "U");
    run.move(37, 5, 6, "R");
    run.move(37, 5, 5, "U");
    run.press("U", 16);
    run.press("A", 12);
    run.wait(80);
    run.require(pallet::view(ctx) == pallet::View::Dialogue,
                "mother conversation uses the game's text");
    verify_bottom_overlay(run, "mother");
    run.require(pallet3d_input_mask() == 255, "conversation neutralizes relative movement");
    capture("mother-dialogue");
    for (int i = 0; i < 12 && pallet::view(ctx) != pallet::View::Overworld; i++)
        run.press("B", 12);
    run.require(pallet::view(ctx) == pallet::View::Overworld, "conversation returns to the room");
    run.move(37, 5, 6, "D");
    run.move(37, 2, 6, "L");
    run.move(37, 2, 2, "U");
    run.move(37, 6, 2, "R");
    run.move(37, 6, 1, "U");
    run.move(38, 7, 1, "R");
    room(38, "reds-house-2f");
    run.move(38, 7, 2, "D");
    run.move(38, 6, 2, "L");
    run.move(38, 6, 1, "U");
    run.move(37, 7, 1, "R");
    room(37, "reds-house-return");
    run.move(37, 6, 1, "L");
    run.move(37, 6, 6, "D");
    run.move(37, 2, 6, "L");
    run.move(0, 5, 6, "D");
    run.wait(35);
    run.require(pallet3d_active() && pallet3d_firstperson() == fp,
                "house exit restores exterior camera");
    capture("pallet-return");
    run.move(0, 9, 6, "R");
    run.move(0, 9, 12, "D");
    run.move(0, 12, 12, "R");
    run.move(40, 5, 11, "U");
    room(40, "oaks-lab");
    run.move(0, 12, 12, "D");
    run.wait(35);
    capture("lab-return");
    run.require(pallet3d_active() && pallet3d_firstperson() == fp,
                "lab exit restores exterior camera");
    std::puts("PASS: house entrance, mother dialogue, stairs round trip, Oak lab, exterior "
              "cameras, GL and unchanged WRAM/VRAM/framebuffer");
    return 0;
}
