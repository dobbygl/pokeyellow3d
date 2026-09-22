#pragma once
#include "options_menu_state.h"

// Settings and input are changed by the original engine. The driver only reads
// their bytes; negative controls operate on a disposable context and restore it.
namespace options_qa {
inline void capture(QaWalk &run, const std::string &label) {
    run.require(options_menu::active(run.ctx) && options_menu::selection(run.ctx).valid,
                "original options menu and dedicated cursor");
    if (pallet3d_menu_style() == ui_preferences::Style::Integrated) {
        auto shown = pallet3d_full_menu();
        run.require(shown.active && !shown.fallback && shown.kind == int(full_menu::Kind::Options),
                    "complete options profile integrates over retained scene");
        full_menu_negative_controls(run);
    }
    auto path = "logs/" + label;
    capture_surface((path + ".ppm").c_str());
    run.require(gb_context_save_state_file(run.ctx, (path + ".state").c_str()),
                "private original options state");
    FILE *file = std::fopen((path + ".tiles").c_str(), "wb");
    run.require(file, "private original options tilemap");
    std::fwrite(run.ctx->wram + 0x3a0, 1, 360, file);
    std::fclose(file);
    std::fprintf(stderr, "[OPTIONS-CAPTURE] %s cursor=%d options=%02x printer=%02x\n",
                 label.c_str(), run.read(options_menu::Cursor), run.read(0xd354), run.read(0xd497));
}
inline void open(QaWalk &run, bool title) {
    if (title) {
        run.require(title_state::sample(run.ctx) == title_state::Phase::Menu,
                    "original initial menu before options");
        for (int i = 0; i < 4 && run.read(0xcc26); ++i)
            run.press("U");
        int last = run.read(0xd087) == 2 ? 2 : 1;
        for (int i = 0; i < last; ++i)
            run.press("D");
    } else {
        if (!start_visible(run.ctx))
            run.press("S");
        run.require(start_visible(run.ctx), "original Start before options");
        for (int i = 0; i < 10 && run.read(0xcc26); ++i)
            run.press("U");
        for (int i = 0; i < 5; ++i)
            run.press("D");
    }
    run.press("A");
    run.wait(90);
    run.require(options_menu::active(run.ctx) && run.read(options_menu::Cursor) == 0,
                "original options opens on Text Speed");
}
inline void pause_and_load(QaWalk &run, const char *label = "OPTIONS") {
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
                    "options freezes pixels and cycles without focus");
    }
    focus.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
    pallet3d_event(&focus, false);
    menu_motion_qa::escape(ctx);
    render(ctx);
    run.require(menu_motion::state.paused && ctx->cycles == cycles,
                "Esc freezes options animation");
    menu_motion_qa::escape(ctx);
    run.require(differences(pixels, render(ctx)) == 0, "Esc restores options pixels");
    run.require(gb_context_save_state_file(ctx, "logs/options-open.state"), "private open save");
    run.wait(8);
    menu_motion_qa::load(ctx, "logs/options-open.state");
    run.require(ctx->cycles == cycles && pallet3d_full_menu().active &&
                    !pallet3d_full_menu().fallback,
                "loaded options integrates without advancing game");
    for (const auto &panel : menu_motion::state.panels)
        if (panel.target)
            run.require(panel.amount == 1, "loaded options panels snap open");
    std::fprintf(stderr, "[%s-MOTION] PASS focus, Esc and open-menu load\n", label);
}
inline int journey(GBContext *ctx, bool fp, bool title) {
    QaWalk run{ctx};
    item_qa::camera(run, fp);
    run.wait(120);
    open(run, title);
    capture(run, "options-first");
    pause_and_load(run);
    // ram_constants.asm and options.asm: original cyclic orders and masks.
    const std::vector<std::vector<int>> values{
        {1, 3, 5}, {0, 128}, {0, 64}, {0, 16, 32, 48}, {0, 32, 64, 96, 127}};
    constexpr int masks[]{15, 128, 64, 48, 255};
    const int original_options = run.read(0xd354), original_printer = run.read(0xd497);
    for (int row = 0; row < 5; ++row) {
        run.require(run.read(options_menu::Cursor) == row, "original option row progression");
        int address = row == 4 ? 0xd497 : 0xd354;
        const auto &order = values[row];
        auto found = std::find(order.begin(), order.end(), run.read(address) & masks[row]);
        run.require(found != order.end(), "source fixture contains a legal original setting");
        int index = int(found - order.begin()), count = int(order.size());
        for (const char *direction : {"R", "L"}) {
            for (int step = 0; step < count; ++step) {
                int before = run.read(address);
                index = (index + (direction[0] == 'R' ? 1 : count - 1)) % count;
                run.press(direction);
                run.require(run.read(address) == ((before & ~masks[row]) | order[index]),
                            "original input cycles setting without changing other bits");
                capture(run, "options-row-" + std::to_string(row) + "-" + direction + "-" +
                                 std::to_string(step));
            }
        }
        run.press("D");
    }
    run.require(run.read(options_menu::Cursor) == 7, "original Down skips dummy rows to Cancel");
    capture(run, "options-cancel");
    run.press("D");
    run.require(run.read(options_menu::Cursor) == 0, "Down wraps Cancel to Text Speed");
    run.press("U");
    run.require(run.read(options_menu::Cursor) == 7, "Up wraps Text Speed to Cancel");
    run.press("U");
    run.require(run.read(options_menu::Cursor) == 4, "Up skips dummy rows to Print");
    run.press("D");
    for (const char *button : {"A", "S", "B"}) {
        run.press(button);
        run.wait(60);
        run.require(!options_menu::active(ctx), "original exit leaves options");
        run.require(title ? title_state::sample(ctx) == title_state::Phase::Menu
                          : start_visible(ctx),
                    "original options returns to its parent menu");
        if (button[0] != 'B')
            open(run, title);
    }
    run.require(run.read(0xd354) == original_options && run.read(0xd497) == original_printer,
                "complete left/right cycles restore all original settings");
    std::puts("PASS: original options, every value in both directions, wrap, Cancel/Start/B");
    return 0;
}
} // namespace options_qa
