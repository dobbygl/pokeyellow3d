#pragma once
#include <string>
#include "title_state.h"
#include "title_picture.h"

// Cold-machine title audit. Optional battery input is read-only; all
// screenshots, traces and machine snapshots belong to the private QA directory.
namespace title_qa {
inline const char *battery_path = nullptr;
inline bool battery_loaded = false;
inline bool load_battery(GBContext *, const char *, void *data, size_t size) {
    FILE *file = battery_path ? std::fopen(battery_path, "rb") : nullptr;
    if (!file)
        return false;
    battery_loaded = std::fread(data, 1, size, file) == size && std::fgetc(file) == EOF;
    std::fclose(file);
    return battery_loaded;
}
inline int audit(GBContext *ctx, const char *mode, int frames) {
    FILE *trace = std::fopen("logs/title-trace.csv", "w");
    if (!trace)
        return 60;
    std::fputs("frame,pc,bank,sp,save,lcdc,bgp,logo,phase,active,stack\n", trace);
    const char *previous_button = nullptr;
    int title_frames = 0, menu_frames = 0, continue_frames = 0, name_frames = 0, world_frames = 0;
    int title_entries = 0, phase_age = 0;
    bool lighting_checked = false;
    auto previous_phase = title_state::Phase::None;
    for (int frame = 0; frame < frames; ++frame) {
        const char *button = nullptr;
        if (std::strcmp(mode, "idle")) {
            if ((frame >= 1500 && frame < 1512) || (frame >= 1750 && frame < 1762))
                button = "START";
            if ((!std::strcmp(mode, "new") || !std::strcmp(mode, "continue")) && frame >= 1900 &&
                frame % 60 < 12)
                button = "A";
        }
        std::string script = std::string("c0:") + (button ? button : "") + ":4294967296";
        if (button != previous_button) {
            gb_platform_set_input_script(button ? script.c_str() : nullptr);
            previous_button = button;
        }
        gb_reset_frame(ctx);
        ctx->stopped = 0;
        unsigned slices = 0;
        while (!ctx->frame_done) {
            if (!gb_platform_poll_events(ctx) || ++slices >= 1000)
                return 61;
            gb_run_cycles(ctx, 70224);
        }
        ReadOnlyMemory before{ctx};
        gb_platform_render_frame(gb_get_framebuffer(ctx));
        if (!before.unchanged(ctx) || glGetError() != GL_NO_ERROR || pallet3d_input_mask() != 255)
            return 62;
        auto phase = title_state::sample(ctx);
        bool phase_changed = phase != previous_phase;
        phase_age = phase_changed ? 0 : phase_age + 1;
        title_entries += phase_changed && phase == title_state::Phase::Title;
        previous_phase = phase;
        title_frames += phase == title_state::Phase::Title;
        menu_frames += phase == title_state::Phase::Menu;
        continue_frames += phase == title_state::Phase::Continue;
        name_frames += phase == title_state::Phase::NewGame;
        world_frames += pallet::view(ctx) == pallet::View::Overworld;
        if (phase != title_state::Phase::None && !pallet3d_active())
            return 66;
        if (!lighting_checked && phase == title_state::Phase::Title && phase_age > 30 &&
            !pallet3d_blend().active) {
            auto settings = pallet3d_daylight_settings();
            auto reference = presentation_qa::render(ctx);
            for (double hour : {0., 6., 12., 18.}) {
                if (!pallet3d_daylight({daynight::Mode::Fixed, hour}) ||
                    presentation_qa::render(ctx) != reference)
                    return 70;
            }
            pallet3d_daylight(settings);
            lighting_checked = true;
            std::fprintf(stderr, "[TITLE] fixed sunset pixels independent of four world hours\n");
        }
        bool logo = title_state::logo(ctx);
        if (frame == 1700 && std::strcmp(mode, "idle")) {
            auto portrait = title_picture::decode(ctx);
            const auto *lcd = gb_get_framebuffer(ctx);
            int ink = 0, different = 0;
            FILE *picture = std::fopen("logs/title-vram-portrait.ppm", "wb");
            if (!picture)
                return 64;
            std::fprintf(picture, "P6\n%d %d\n255\n", title_picture::Width, title_picture::Height);
            for (int y = 0; y < title_picture::Height; ++y)
                for (int x = 0; x < title_picture::Width; ++x) {
                    int p = (y * title_picture::Width + x) * 4;
                    uint8_t rgb[]{255, 255, 255};
                    if (portrait[p + 3]) {
                        ++ink;
                        uint32_t original =
                            lcd[(y + title_picture::Top) * 160 + x + title_picture::Left];
                        uint32_t decoded = (uint32_t(portrait[p]) << 16) |
                                           (uint32_t(portrait[p + 1]) << 8) | portrait[p + 2];
                        different += decoded != (original & 0xffffff);
                        std::memcpy(rgb, portrait.data() + p, 3);
                    }
                    std::fwrite(rgb, 1, 3, picture);
                }
            std::fclose(picture);
            std::fprintf(stderr, "[TITLE] VRAM portrait oracle ink=%d differences=%d\n", ink,
                         different);
            if (ink < 1000 || different)
                return 65;
        }
        std::fprintf(trace, "%d,%04x,%02x,%04x,%02x,%02x,%02x,%d,%d,%d,", frame, ctx->pc,
                     ctx->hram[0x38], ctx->sp, ctx->wram[0x1087], ctx->io[0x40], ctx->io[0x47],
                     logo, int(title_state::sample(ctx)), pallet3d_active());
        for (int a = ctx->sp; a >= 0xd000 && a < 0xdfff; a += 2)
            std::fprintf(trace, "%04x ", ctx->wram[a - 0xc000] | (ctx->wram[a - 0xbfff] << 8));
        std::fputc('\n', trace);
        if (frame % 120 == 0 || frame == frames - 1 || phase_changed || phase_age == 16) {
            char path[128];
            std::snprintf(path, sizeof(path), "logs/title-%04d.ppm", frame);
            capture_surface(path);
        }
    }
    std::fclose(trace);
    std::fprintf(stderr, "[TITLE] phases title=%d menu=%d continue=%d new=%d world=%d battery=%d\n",
                 title_frames, menu_frames, continue_frames, name_frames, world_frames,
                 battery_loaded);
    if (!title_frames || !lighting_checked ||
        (!std::strcmp(mode, "continue") && (!battery_loaded || !continue_frames || !world_frames)))
        return 67;
    if (!std::strcmp(mode, "idle") && frames >= 7200 && title_entries < 2)
        return 68;
    if (!std::strcmp(mode, "new") && frames >= 6000 &&
        (!world_frames || ctx->wram[0x1157] != 0x80 || ctx->wram[0x1158] != 0x80))
        return 69;
    if (!gb_context_save_state_file(ctx, "logs/title-final.state"))
        return 63;
    std::fprintf(stderr,
                 "[TITLE] audited %d original frames (%s), read-only memory and neutral controls\n",
                 frames, mode);
    return 0;
}
} // namespace title_qa
