#pragma once

namespace naming_qa {
inline bool intro_keyboard(GBContext *ctx, int type) {
    return type >= 0 && type <= 1 && ctx->wram[0x107c] == type && ctx->io[0x47] == 0xe4 &&
           item_menu::live_call(ctx, type ? 0x6747 : 0x66ff, 0x6307) &&
           battle::tile(ctx, 0, 4) == 0x79 && battle::tile(ctx, 19, 14) == 0x7e &&
           battle::tile(ctx, 1, 5) == 0xed;
}
inline void source_capture(QaWalk &run, const char *label) {
    std::string path = std::string("logs/") + label;
    capture_surface((path + ".ppm").c_str());
    run.require(gb_context_save_state_file(run.ctx, (path + ".state").c_str()),
                "private original naming state");
    FILE *file = std::fopen((path + ".tiles").c_str(), "wb");
    run.require(file, "private original naming tilemap");
    std::fwrite(run.ctx->wram + 0x3a0, 1, 360, file);
    std::fclose(file);
}
inline int prepare(GBContext *ctx) {
    QaWalk run{ctx};
    run.wait(120);
    run.require(title_state::sample(ctx) == title_state::Phase::Menu && run.read(0xd087) == 1,
                "initial menu without a save before original new-game naming");
    for (int i = 0; i < 4 && run.read(0xcc26); ++i)
        run.press("U");
    for (int type = 0; type < 2; ++type) {
        for (int attempt = 0; attempt < 400 && !intro_keyboard(ctx, type); ++attempt)
            run.press("A");
        run.require(intro_keyboard(ctx, type), "original Oak dialogue opens custom naming");
        run.wait(30);
        run.require(run.read(0xcee9) == 0, "new player/rival naming starts empty");
        source_capture(run, type ? "naming-rival-empty" : "naming-player-empty");
        if (!type) {
            run.press("A");
            run.require(run.read(0xcee9) == 1 && run.read(0xcf4a) == 0x80,
                        "original player naming appends A");
            run.press("S");
            run.wait(60);
            run.require(run.read(0xd157) == 0x80 && run.read(0xd158) == 0x50,
                        "original Start submits custom player name");
        }
    }
    std::puts("PASS: original new-game dialogue and empty custom player/rival naming fixtures");
    return 0;
}
} // namespace naming_qa
