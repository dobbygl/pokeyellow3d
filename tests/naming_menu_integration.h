#pragma once
#include "naming_menu_state.h"

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
inline int prepare_capture(GBContext *ctx) {
    QaWalk run{ctx};
    run.require(run.read(pallet::Battle) == 1, "original wild capture in progress");
    for (int attempt = 0; attempt < 300 && !naming_menu::active(ctx); ++attempt)
        run.press("A");
    run.wait(30);
    run.require(naming_menu::active(ctx) && item_menu::live_call(ctx, 0x62a3, 0x6307) &&
                    run.read(naming_menu::Type) == 2 && run.read(naming_menu::Length) == 0,
                "original successful capture opens empty AskName keyboard");
    source_capture(run, "naming-capture-empty");
    std::puts("PASS: original wild capture opens empty nickname keyboard");
    return 0;
}
inline std::pair<int, int> cursor(QaWalk &run) {
    int pointer = run.read(0xcc30) | (run.read(0xcc31) << 8);
    int cell = pointer - 0xc3a0;
    run.require(cell >= 0 && cell < 360 && run.read(pointer) == 0xed,
                "original naming cursor pointer identifies the painted arrow");
    return {cell % 20, cell / 20};
}
inline void capture(QaWalk &run, const char *label) {
    run.require(naming_menu::active(run.ctx), "original naming call remains active");
    if (pallet3d_menu_style() == ui_preferences::Style::Integrated) {
        auto shown = pallet3d_full_menu();
        run.require(shown.active && !shown.fallback && shown.kind == int(full_menu::Kind::Naming),
                    "original naming keyboard integrates completely");
        full_menu_negative_controls(run);
    }
    source_capture(run, label);
}
inline int journey(GBContext *ctx, bool fp, bool submit_end) {
    QaWalk run{ctx};
    item_qa::camera(run, fp);
    run.wait(30);
    int type = run.read(naming_menu::Type), maximum = type == 2 ? 10 : 7;
    run.require(type <= 2 && run.read(naming_menu::Length) == 0 && !run.read(naming_menu::Case),
                "empty uppercase original naming fixture");
    bool captured = item_menu::live_call(ctx, 0x62a3, 0x6307);
    int destination = type == 0 ? 0xd157
                      : type == 1
                          ? 0xd349
                          : 0xd2b4 + (captured ? run.read(0xd162) - 1 : run.read(0xcf91)) * 11;
    capture(run, "naming-first");
    options_qa::pause_and_load(run, "NAMING");
    if (type == 2) {
        std::array<uint8_t, 16> first{};
        std::copy_n(ctx->oam, first.size(), first.begin());
        int changed = 0;
        auto cycles = ctx->cycles;
        for (int frame = 0; frame < 120; ++frame) {
            run.tick();
            changed += !std::equal(first.begin(), first.end(), ctx->oam);
        }
        run.require(changed > 0 && changed < 120 && ctx->cycles > cycles,
                    "original Pokemon icon animates with guest cycles");
        std::fprintf(stderr, "[NAMING-ICON] 120 frames, %d differing original OAM phases\n",
                     changed);
    }
    if (pallet3d_menu_style() == ui_preferences::Style::Integrated) {
        for (auto probe : {std::pair<int, const char *>{0xf00, "end"},
                           {0x1760, "underscore"},
                           {0x1770, "raised-underscore"}}) {
            ctx->vram[probe.first] ^= 1;
            ReadOnlyMemory memory{ctx};
            gb_platform_render_frame(gb_get_framebuffer(ctx));
            run.require(memory.unchanged(ctx) && pallet3d_full_menu().fallback,
                        "unknown naming graphic preserves complete LCD");
            ctx->vram[probe.first] ^= 1;
            gb_platform_render_frame(gb_get_framebuffer(ctx));
            run.require(!pallet3d_full_menu().fallback, "restored naming graphic integrates");
            std::fprintf(stderr, "[NAMING-NEGATIVE] %s full LCD verified\n", probe.second);
        }
        if (type == 2) {
            int address = ctx->oam[2] * 16;
            ctx->vram[address] ^= 1;
            ReadOnlyMemory memory{ctx};
            gb_platform_render_frame(gb_get_framebuffer(ctx));
            run.require(memory.unchanged(ctx) && pallet3d_full_menu().fallback,
                        "unverified original icon preserves complete LCD");
            ctx->vram[address] ^= 1;
            gb_platform_render_frame(gb_get_framebuffer(ctx));
            run.require(!pallet3d_full_menu().fallback, "restored icon integrates");
            std::puts("[NAMING-NEGATIVE] icon full LCD verified");
            ctx->oam[1] += 1;
            ReadOnlyMemory geometry{ctx};
            gb_platform_render_frame(gb_get_framebuffer(ctx));
            run.require(geometry.unchanged(ctx) && pallet3d_full_menu().fallback,
                        "unknown naming sprite placement preserves complete LCD");
            ctx->oam[1] -= 1;
            gb_platform_render_frame(gb_get_framebuffer(ctx));
            run.require(!pallet3d_full_menu().fallback, "restored icon geometry integrates");
            std::puts("[NAMING-NEGATIVE] icon-geometry full LCD verified");
        }
    }
    run.press("B");
    run.require(run.read(naming_menu::Length) == 0, "B preserves an empty original name");
    run.press("L");
    run.require(cursor(run) == std::make_pair(17, 5), "Left wraps the original nine columns");
    run.press("R");
    run.require(cursor(run) == std::make_pair(1, 5), "Right wraps to the first original column");
    run.press("U");
    run.require(cursor(run) == std::make_pair(1, 15), "Up wraps to the original case control");
    run.press("L");
    run.press("R");
    run.require(cursor(run) == std::make_pair(1, 15), "case control ignores horizontal input");
    run.press("A");
    run.require(run.read(naming_menu::Case) == 1, "A on case control selects lowercase");
    capture(run, "naming-lowercase");
    run.press("D");
    run.require(cursor(run) == std::make_pair(1, 5), "Down wraps case control to first row");
    run.press("A");
    run.require(run.read(0xcf4a) == 0xa0 && run.read(naming_menu::Length) == 1,
                "original lowercase a appended");
    run.press("T");
    run.require(run.read(naming_menu::Case) == 0, "Select returns original uppercase alphabet");
    run.press("R");
    run.press("A");
    run.require(run.read(0xcf4b) == 0x81 && run.read(naming_menu::Length) == 2,
                "original uppercase B appended after lowercase a");
    capture(run, "naming-mixed-case");
    run.press("B");
    run.require(run.read(naming_menu::Length) == 1 && run.read(0xcf4b) == 0x50,
                "B deletes exactly the final original character");
    for (int row = 2; row <= 6; ++row) {
        run.press("D");
        run.require(cursor(run).second == 3 + row * 2, "original vertical keyboard rows");
        if (row == 4)
            capture(run, "naming-symbol-row");
    }
    run.require(cursor(run) == std::make_pair(1, 15), "entering case control forces first column");
    run.press("D");
    for (int length = 1; length < maximum; ++length) {
        run.press("A");
        run.require(run.read(naming_menu::Length) == length + 1, "original name grows to capacity");
    }
    run.require(cursor(run) == std::make_pair(17, 13), "capacity forces original ED selection");
    capture(run, "naming-capacity");
    // Move from ED to a letter; the engine refuses another character and
    // returns to ED. No name bytes are supplied or changed by the driver.
    for (int i = 0; i < 4; ++i)
        run.press("U");
    run.press("R");
    run.press("A");
    run.require(run.read(naming_menu::Length) == maximum && cursor(run) == std::make_pair(17, 13),
                "original capacity check refuses an extra character");
    run.press("B");
    run.require(run.read(naming_menu::Length) == maximum - 1, "delete from a full name");
    capture(run, "naming-deleted");
    std::vector<uint8_t> expected(ctx->wram + 0xf4a, ctx->wram + 0xf4a + maximum);
    run.press(submit_end ? "A" : "S");
    run.wait(60);
    run.require(!naming_menu::active(ctx), "original Start/ED submits and closes keyboard");
    run.require(std::equal(expected.begin(), expected.end(), ctx->wram + destination - 0xc000),
                "original naming routine writes precisely the selected name");
    source_capture(run, "naming-submitted");
    std::puts("PASS: original naming, cases, keyboard wraps, editing, limits and submission");
    return 0;
}
} // namespace naming_qa
