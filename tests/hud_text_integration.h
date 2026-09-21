#pragma once

// Stress the presentation adapter with every original trainer-class label.
// A real engine-produced intro supplies its VRAM, portrait and LCD. Only the
// disposable fixture's name buffer changes, before each read-only snapshot.
namespace hud_text_qa {
inline int trainer_labels(GBContext *ctx, const uint8_t *rom, bool first_person) {
    QaWalk run{ctx};
    run.require(battle::trainer_intro(ctx), "original trainer-introduction fixture");
    if (first_person) {
        SDL_Event event{};
        event.type = SDL_KEYDOWN;
        event.key.keysym.scancode = SDL_SCANCODE_F3;
        run.require(pallet3d_event(&event, false) && pallet3d_firstperson(),
                    "trainer label first-person camera");
    }
    std::array<uint8_t, 13> original{};
    std::copy_n(ctx->wram + 0x1049, original.size(), original.begin());
    // pokeyellow_rom.c: GB_ROM_PTR_TrainerNames = rom_data + 235902.
    size_t address = 235902;
    int longest = 0;
    for (int trainer = 1; trainer <= 47; ++trainer) {
        size_t end = address;
        while (end - address < original.size() && rom[end] != 0x50)
            ++end;
        size_t length = end - address;
        run.require(length > 0 && length < original.size(), "bounded original trainer label");
        longest = std::max(longest, int(length));
        std::fill_n(ctx->wram + 0x1049, original.size(), 0x50);
        std::copy_n(rom + address, length, ctx->wram + 0x1049);
        ReadOnlyMemory memory{ctx};
        size_t observed = ui_style_qa::trainer_captions;
        for (int frame = 0; frame < 4; ++frame) {
            gb_platform_render_frame(gb_get_framebuffer(ctx));
            run.require(memory.unchanged(ctx) && glGetError() == GL_NO_ERROR,
                        "trainer label rendering leaves all game memory unchanged");
        }
        run.require(ui_style_qa::trainer_captions > observed,
                    "original trainer label checked for omissions and pixel bits");
        if (trainer == 2 || trainer == 31 || trainer == 32) {
            char path[80];
            std::snprintf(path, sizeof path, "logs/trainer-label-%02d.ppm", trainer);
            capture_surface(path);
        }
        address = end + 1;
    }
    run.require(longest == 12 && ui_style_qa::long_trainer_captions > 0,
                "longest original names include their final gender glyph");
    // A special glyph beyond the old ten-cell limit must reject the whole
    // caption, rather than silently dropping that part of the original name.
    std::fill_n(ctx->wram + 0x1049, 12, 0x80);
    ctx->wram[0x1049 + 11] = 0x6e;
    ctx->wram[0x1049 + 12] = 0x50;
    ReadOnlyMemory memory{ctx};
    gb_platform_render_frame(gb_get_framebuffer(ctx));
    run.require(memory.unchanged(ctx) && pallet3d_battle().full_overlay &&
                    pallet3d_battle().overlay_alpha == 1 && glGetError() == GL_NO_ERROR,
                "extra-font suffix preserves the complete native LCD fallback");
    std::copy(original.begin(), original.end(), ctx->wram + 0x1049);
    std::puts("PASS: all 47 original trainer labels, complete twelve-glyph names, ROM bits, "
              "unsupported suffix fallback and read-only rendering");
    return 0;
}
} // namespace hud_text_qa
