#pragma once
#include "pc_details.h"
namespace pc_details_qa {
inline void observe(GBContext *ctx, int frame) {
    observe_menu_frame(ctx, frame);
    auto shown = pallet3d_pc_details();
    auto fail = [&](bool ok, const char *why) {
        if (!ok) {
            std::fprintf(stderr, "[PC-DETAILS] FAIL frame=%d %s\n", frame, why);
            std::exit(52);
        }
    };
    if (shown.items) {
        fail(shown.stored == battle::read(ctx, 0xd539), "stored counter matches original slots");
        fail(pallet3d_pc().mode == int(pc_state::Mode::Items),
             "item counter is scoped to player PC");
    }
    if (shown.oak) {
        int counts[2]{};
        for (int j = 0; j < 2; j++)
            for (int i = 0; i < 19; i++) {
                unsigned byte = battle::read(ctx, (j ? 0xd2f6 : 0xd309) + i);
                while (byte) {
                    counts[j] += int(byte & 1);
                    byte >>= 1;
                }
            }
        fail(shown.seen == counts[0] && shown.caught == counts[1],
             "Oak counters match original bitmaps");
        fail(pallet3d_pc().monitor, "Oak text is on the monitor");
    }
    fail(pallet3d_input_mask() == 255, "relative controls remain neutral");
}
inline void verify_counter(QaWalk &run) {
    auto details = pallet3d_pc_details();
    run.require(pallet3d_pc().monitor && (details.items || details.oak), "focused PC has counters");
    char value[32];
    if (details.items)
        std::snprintf(value, sizeof(value), "ITEMS %02d OF 50", run.read(0xd539));
    else {
        auto rating = pc_details::rating(run.ctx);
        std::snprintf(value, sizeof(value), "SEEN %03d CAUGHT %03d", rating.seen, rating.caught);
    }
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int w = viewport[2], h = viewport[3], checked = 0;
    std::vector<uint8_t> rgba(size_t(w) * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    auto r = details.counter_screen;
    for (size_t i = 0; value[i]; ++i) {
        int glyph = value[i] >= '0' && value[i] <= '9'   ? 0x76 + value[i] - '0'
                    : value[i] >= 'A' && value[i] <= 'Z' ? value[i] - 'A'
                                                         : -1;
        if (glyph < 0)
            continue;
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                float u = (i * 8 + x + .5f) / (std::strlen(value) * 8), v = (y + .5f) / 8;
                int px = int(r[0] * (1 - u) * (1 - v) + r[2] * u * (1 - v) + r[4] * u * v +
                             r[6] * (1 - u) * v);
                int py = int(r[1] * (1 - u) * (1 - v) + r[3] * u * (1 - v) + r[5] * u * v +
                             r[7] * (1 - u) * v);
                run.require(px >= 0 && px < w && py >= 0 && py < h, "entire counter is visible");
                bool ink = run.ctx->rom[0x10600 + glyph * 8 + y] & (1 << (7 - x));
                bool bright = rgba[((h - 1 - py) * w + px) * 4] > 190;
                run.require(ink == bright,
                            "every counter glyph is visible and matches original ROM font");
                ++checked;
            }
    }
    std::fprintf(stderr, "[PC-DETAILS] visible counter '%s': %d glyph pixels checked\n", value,
                 checked);
}
inline void center(pc_qa::Session &session) {
    auto &run = session.run;
    for (int i = 0; i < 12 && pc_state::sample(run.ctx).mode != pc_state::Mode::Center; i++) {
        run.press("B");
        run.wait(60);
    }
    session.until(
        [&] {
            return pc_state::sample(run.ctx).mode == pc_state::Mode::Center &&
                   run.read(0xcc28) == (run.read(0xd5a1) ? 4 : 3);
        },
        "return to Center PC menu");
}
inline void choose_center(pc_qa::Session &session, int choice) {
    auto &run = session.run;
    for (int i = 0; i < 8 && run.read(0xcc26); i++)
        run.press("U");
    for (int i = 0; i < choice; i++)
        run.press("D");
    run.press("A");
    run.wait(100);
}
inline void wait_items(pc_qa::Session &session) {
    auto &run = session.run;
    auto ready = [&] {
        auto sample = pc_state::sample(run.ctx);
        return sample.mode == pc_state::Mode::Items && sample.main;
    };
    for (int i = 0; i < 16 && !ready(); ++i) {
        run.press("B");
        run.wait(80);
    }
    run.require(ready(), "player PC main menu");
    run.wait(35);
}
inline void choose_items(pc_qa::Session &session, int choice) {
    auto &run = session.run;
    wait_items(session);
    for (int i = 0; i < 8 && run.read(0xcc26); i++)
        run.press("U");
    for (int i = 0; i < choice; i++)
        run.press("D");
    run.press("A");
    run.wait(100);
}
inline int quantity(QaWalk &run, int count_address, int item) {
    for (int i = 0; i < run.read(count_address); i++)
        if (run.read(count_address + 1 + 2 * i) == item)
            return run.read(count_address + 2 + 2 * i);
    return 0;
}
inline void select_item(QaWalk &run, int count_address, int item) {
    int index = -1;
    for (int i = 0; i < run.read(count_address); i++)
        if (run.read(count_address + 1 + 2 * i) == item)
            index = i;
    run.require(index >= 0, "original inventory contains requested item");
    for (int i = 0; i < index; i++)
        run.press("D");
    run.press("A");
    run.wait(80);
}
inline void details(pc_qa::Session &session) {
    auto &run = session.run;
    auto *ctx = run.ctx;
    qa_frame_observer = observe;
    center(session);
    choose_center(session, 1);
    wait_items(session);
    run.require(pallet3d_pc_details().items, "player PC counter visible");
    verify_menu_overlay(run, "items-main", 2);
    verify_counter(run);
    int bag = quantity(run, 0xd31c, 4), stored = quantity(run, 0xd539, 4);
    run.require(bag > 0, "private fixture owns Poke Balls");
    int slots = run.read(0xd539);
    choose_items(session, 1);
    verify_menu_overlay(run, "items-deposit-list");
    select_item(run, 0xd31c, 4);
    verify_menu_overlay(run, "items-deposit-quantity");
    run.press("A");
    run.wait(180);
    session.until([&] { return quantity(run, 0xd539, 4) == stored + 1; },
                  "original engine deposits one Poke Ball");
    run.require(quantity(run, 0xd31c, 4) == bag - 1 && run.read(0xd539) == slots + (stored == 0),
                "deposit changes original quantities and slot counter");
    verify_menu_overlay(run, "items-deposited");
    run.press("B");
    run.wait(80);
    wait_items(session);
    choose_items(session, 0);
    select_item(run, 0xd539, 4);
    verify_menu_overlay(run, "items-withdraw-quantity");
    run.press("A");
    run.wait(180);
    session.until([&] { return quantity(run, 0xd539, 4) == stored; },
                  "original engine withdraws one Poke Ball");
    run.require(quantity(run, 0xd31c, 4) == bag && run.read(0xd539) == slots,
                "withdraw restores original counts");
    verify_menu_overlay(run, "items-withdrawn");
    wait_items(session);
    choose_items(session, 1);
    select_item(run, 0xd31c, 4);
    run.press("A");
    run.wait(180);
    session.until([&] { return quantity(run, 0xd539, 4) == stored + 1; },
                  "deposit a ball for original toss flow");
    wait_items(session);
    choose_items(session, 2);
    verify_menu_overlay(run, "items-toss-list");
    select_item(run, 0xd539, 4);
    verify_menu_overlay(run, "items-toss-quantity");
    run.press("A");
    run.wait(100);
    verify_menu_overlay(run, "items-toss-confirm");
    run.press("A");
    run.wait(180);
    session.until([&] { return quantity(run, 0xd539, 4) == stored; },
                  "original confirmed toss removes one ball");
    run.require(quantity(run, 0xd31c, 4) == bag - 1 && run.read(0xd539) == slots,
                "toss removes only the stored ball after confirmation");
    verify_menu_overlay(run, "items-tossed");
    center(session);
    choose_center(session, 2);
    for (int i = 0; i < 18; i++) {
        auto state = pc_state::sample(ctx);
        if (state.mode == pc_state::Mode::Oak && run.read(0xcc28) == 1 &&
            battle::tile(ctx, 14, 7) == 0x79)
            break;
        run.press("A");
        run.wait(80);
    }
    run.require(pc_state::sample(ctx).mode == pc_state::Mode::Oak, "Oak PC remains active");
    verify_menu_overlay(run, "oak-question");
    run.press("A");
    run.wait(150);
    run.require(pallet3d_pc_details().oak && pallet3d_pc().monitor,
                "Oak evaluation and counts on monitor");
    verify_menu_overlay(run, "oak-evaluation");
    verify_counter(run);
    // Let the original text pages advance; no rating or cursor is injected.
    for (int i = 0; i < 8 && pc_state::sample(ctx).mode == pc_state::Mode::Oak; i++) {
        run.press("A");
        run.wait(100);
        if (pc_state::sample(ctx).mode == pc_state::Mode::Oak)
            verify_menu_overlay(run, (std::string("oak-page-") + std::to_string(i)).c_str());
    }
    std::puts(
        "PASS: original item deposit/withdraw/toss and Oak evaluation, live counters, exact menus "
        "and read-only frames");
}
inline int items_and_oak(GBContext *ctx, bool fp) {
    pc_qa::Session session(ctx, fp);
    details(session);
    session.close();
    return 0;
}
} // namespace pc_details_qa
