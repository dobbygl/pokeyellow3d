#pragma once
namespace pc_hall_qa {
// Explicit private champion fixture: retain the actually caught party and use
// the original SaveHallOfFameTeams routine to write its record to SRAM. This
// does not claim a completed playthrough, and is never part of the renderer.
inline void prepare(GBContext *ctx) {
    QaWalk run{ctx};
    run.wait(30);
    auto party = pc_storage::read(ctx).party;
    run.require(party.valid && party.count > 0, "champion fixture has an actual party");
    run.require(gb_context_save_state_file(ctx, "logs/pre-champion.state"), "fixture checkpoint");
    std::array<uint8_t, 96> record{};
    for (int i = 0; i < party.count; ++i) {
        record[i * 16] = uint8_t(party.mons[i].species);
        record[i * 16 + 1] = uint8_t(party.mons[i].level);
        std::copy(party.mons[i].name.begin(), party.mons[i].name.end(),
                  record.begin() + i * 16 + 2);
    }
    if (party.count < 6)
        record[party.count * 16] = 255;
    std::copy(record.begin(), record.end(), ctx->wram + 0xc5b);
    ctx->wram[0x15a1] = 1;
    gb_write8(ctx, 0x2000, 0x1c);
    ctx->pc = 0x7e2e;
    ctx->sp = 0xdff0;
    ctx->ime = ctx->ime_pending = ctx->halted = ctx->halt_bug = 0;
    ctx->single_step_mode = 1;
    gb_push16(ctx, 0);
    for (int i = 0; i < 50000 && ctx->pc; ++i) {
        ctx->stopped = 0;
        gb_step(ctx);
    }
    run.require(!ctx->pc && !std::memcmp(ctx->eram + 0x598, record.data(), record.size()),
                "original SaveHallOfFameTeams writes the actual party to SRAM");
    std::vector<uint8_t> saved(ctx->eram, ctx->eram + ctx->eram_size);
    run.require(gb_context_load_state_file(ctx, "logs/pre-champion.state"),
                "restore private world");
    std::copy(saved.begin(), saved.end(), ctx->eram);
    ctx->wram[0x15a1] = 1;
    run.require(pc_details::hall_team(ctx, 0).valid, "saved champion team parses");
    run.require(gb_context_save_state_file(ctx, "logs/champion.state"), "private champion fixture");
}
inline void observe(GBContext *ctx, int frame) {
    observe_menu_frame(ctx, frame);
    auto hall = pallet3d_hall();
    if (hall.active) {
        auto original = pc_details::hall(ctx);
        if (!original.valid || hall.team != original.index || hall.selected != original.selected ||
            hall.count != original.count || hall.fingerprint != original.fingerprint ||
            pallet3d_input_mask() != 255) {
            std::fprintf(stderr, "[HALL] FAIL stale team/selection or relative controls frame=%d\n",
                         frame);
            std::exit(53);
        }
    }
}
inline void verify(QaWalk &run) {
    auto *ctx = run.ctx;
    auto hall = pallet3d_hall();
    run.require(hall.active, "Hall gallery is actually presented");
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int w = viewport[2], h = viewport[3];
    std::vector<uint8_t> pixels(size_t(w) * h * 4), again(pixels.size());
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    ReadOnlyMemory before{ctx};
    for (int i = 0; i < 3; ++i)
        gb_platform_render_frame(gb_get_framebuffer(ctx));
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, again.data());
    run.require(before.unchanged(ctx) && pixels == again && pallet3d_hall().builds == hall.builds &&
                    pallet3d_hall().uploads == hall.uploads,
                "frozen Hall frames reuse geometry and textures");
    int scale = std::max(1, std::min(w / 264, h / 300));
    int left = (w - 264 * scale) / 2, top = h - (88 * scale + 24) + 12;
    int checked = 0;
    auto region = [&](int x, int y, int rw, int rh, int dx, int dy) {
        for (int yy = 0; yy < rh * 8; ++yy)
            for (int xx = 0; xx < rw * 8; ++xx) {
                int sx = dx + xx * scale + scale / 2, sy = dy + yy * scale + scale / 2;
                auto *p = &pixels[((h - 1 - sy) * w + sx) * 4];
                auto original = gb_get_framebuffer(ctx)[(y * 8 + yy) * 160 + x * 8 + xx];
                run.require(p[0] == (original >> 16 & 255) && p[1] == (original >> 8 & 255) &&
                                p[2] == (original & 255),
                            "original Hall text pixels are exact");
                ++checked;
            }
    };
    region(0, 2, 12, 11, left, top);
    region(0, 13, 20, 4, left + 104 * scale, top + 28 * scale);
    std::fprintf(stderr, "[HALL] verified %d original text pixels, team=%d member=%d\n", checked,
                 hall.team, hall.selected);
}
inline void show(pc_qa::Session &session) {
    auto &run = session.run;
    auto *ctx = run.ctx;
    pc_details_qa::center(session);
    qa_frame_observer = observe;
    pc_details_qa::choose_center(session, 3);
    session.until([&] { return pc_details::hall(ctx).valid; }, "original Hall team display");
    auto team = pc_details::hall(ctx);
    for (int i = 0; i < team.count; ++i) {
        run.wait(50);
        verify(run);
        auto current = pc_details::hall(ctx);
        run.require(current.valid && current.selected == i, "original selected Hall member");
        auto pic = mon_pic::front(ctx->rom, ctx->rom_size, current.mons[i].species);
        run.require(pic.valid &&
                        !std::memcmp(pic.tiles.data(), ctx->vram + 0x1000, mon_pic::TileBytes),
                    "Hall portrait decoder equals live VRAM");
        std::string stem = "logs/hall-member-" + std::to_string(i);
        capture_surface((stem + ".ppm").c_str());
        run.require(gb_context_save_state_file(ctx, (stem + ".state").c_str()), "Hall evidence");
        if (i == 0) {
            uint8_t saved = ctx->eram[0x598];
            ctx->eram[0x598] = 0;
            run.wait(2);
            run.require(!pallet3d_hall().active && pallet3d_pc().active,
                        "corrupt SRAM falls back to the original PC image");
            verify_menu_overlay(run, "hall-corrupt-fallback");
            ctx->eram[0x598] = saved;
            run.wait(2);
            verify(run);
            run.require(gb_context_load_state_file(ctx, (stem + ".state").c_str()),
                        "cold Hall load");
            run.wait(2);
            verify(run);
        }
        if (i + 1 < team.count) {
            run.press("A");
            session.until(
                [&] {
                    auto next = pc_details::hall(ctx);
                    return next.valid && next.selected == i + 1;
                },
                "next original Hall member");
        }
    }
    std::puts("PASS: private original-engine Hall SRAM, live selections and VRAM portraits");
}
inline int journey(GBContext *ctx, bool fp, bool complete = false) {
    prepare(ctx);
    pc_qa::Session session(ctx, fp);
    if (complete) {
        session.run.require(session.run.read(0xd162) >= 2 && session.run.read(0xda7f) == 0,
                            "complete PC fixture has two party members and empty box");
        session.deposit(1);
        session.change(6);
        session.change(0);
        session.withdraw(0);
        pc_details_qa::details(session);
    }
    show(session);
    session.close();
    return 0;
}
} // namespace pc_hall_qa
