#pragma once
#include "trainer_card_state.h"

namespace trainer_card_qa {
inline void capture(QaWalk &run, const char *label) {
    run.require(trainer_card::active(run.ctx), "original trainer-card call");
    if (pallet3d_menu_style() == ui_preferences::Style::Integrated) {
        auto shown = pallet3d_full_menu();
        run.require(shown.active && !shown.fallback &&
                        shown.kind == int(full_menu::Kind::TrainerCard),
                    "complete trainer card integrates original text and graphics");
        full_menu_negative_controls(run);
    }
    verify_menu_overlay(run, label);
}
inline int journey(GBContext *ctx, bool fp, int badges) {
    QaWalk run{ctx};
    item_qa::camera(run, fp);
    run.require(pallet::view(ctx) == pallet::View::Overworld, "world before private badge fixture");
    ctx->wram[0x1355] = uint8_t(badges);
    // Bounded private variants exercise original numeric formatting too.
    if (badges) {
        std::fill_n(ctx->wram + 0x1157, 7, uint8_t(0x96)); // Seven original W characters.
        ctx->wram[0x115e] = 0x50;
        ctx->wram[0x1346] = ctx->wram[0x1347] = ctx->wram[0x1348] = badges == 255 ? 0x99 : 0;
        ctx->wram[0x1a40] = badges == 255 ? 255 : 10;
        ctx->wram[0x1a42] = badges == 255 ? 59 : 0;
    }
    qa_frame_observer = observe_menu_frame;
    auto open = [&]() {
        if (!start_visible(ctx))
            run.press("S");
        run.require(start_visible(ctx), "original Start before trainer card");
        for (int i = 0; i < 10 && run.read(0xcc26); ++i)
            run.press("U");
        for (int i = 0; i < 3; ++i)
            run.press("D");
        run.press("A");
        run.wait(90);
    };
    open();
    capture(run, "trainer-card-first");
    options_qa::pause_and_load(run, "TRAINER-CARD");
    if (pallet3d_menu_style() == ui_preferences::Style::Integrated) {
        for (auto probe : {std::pair<int, const char *>{0x1000, "portrait"},
                           {0x1200, "face-or-badge"},
                           {0xd80, "badge-number"},
                           {0xd60, "time-colon"},
                           {0xd70, "background"},
                           {0x1760, "circle"},
                           {0x1770, "border"}}) {
            // For obtained badges the first visible icon is the badge at 24.
            int address = probe.first == 0x1200 && (badges & 1) ? 0x1240 : probe.first;
            auto original = ctx->vram[address];
            ctx->vram[address] ^= 1;
            ReadOnlyMemory memory{ctx};
            gb_platform_render_frame(gb_get_framebuffer(ctx));
            run.require(memory.unchanged(ctx) && pallet3d_full_menu().active &&
                            pallet3d_full_menu().fallback,
                        "unverified trainer graphic retains full original LCD");
            ctx->vram[address] = original;
            ReadOnlyMemory restored{ctx};
            gb_platform_render_frame(gb_get_framebuffer(ctx));
            run.require(restored.unchanged(ctx) && !pallet3d_full_menu().fallback,
                        "restored trainer graphic integrates immediately");
            std::fprintf(stderr, "[TRAINER-CARD-NEGATIVE] %s full LCD verified\n", probe.second);
        }
        ctx->wram[0x1355] ^= 1;
        ReadOnlyMemory memory{ctx};
        gb_platform_render_frame(gb_get_framebuffer(ctx));
        run.require(memory.unchanged(ctx) && pallet3d_full_menu().fallback,
                    "badge/face disagreement retains original LCD");
        ctx->wram[0x1355] ^= 1;
        gb_platform_render_frame(gb_get_framebuffer(ctx));
    }
    for (const char *button : {"A", "B"}) {
        run.press(button);
        run.wait(60);
        run.require(!trainer_card::active(ctx) && start_visible(ctx),
                    "original trainer card returns to Start");
        if (button[0] == 'A') {
            open();
            capture(run, "trainer-card-reopened");
        }
    }
    run.require(run.read(0xd355) == badges, "reading trainer card preserves obtained badges");
    item_qa::close(run);
    std::puts("PASS: original trainer card, badge variant, source graphics, A/B and return");
    return 0;
}
} // namespace trainer_card_qa
