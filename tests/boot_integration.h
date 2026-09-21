#pragma once
#include "title_state.h"
#include <algorithm>
#include <string>
#include <vector>

// Full, unskipped cold boot. Only the original joypad is driven; presentation
// reads are guarded every frame. Videos and traces belong in private build/qa.
namespace boot_qa {
inline void require(bool ok, const char *message) {
    if (!ok) {
        std::fprintf(stderr, "[BOOT] FAIL: %s\n", message);
        std::exit(70);
    }
}
struct Movie {
    FILE *pipe = nullptr;
    ~Movie() {
        close();
    }
    void open(int width, int height, const char *name, bool flip) {
        std::string command = "ffmpeg -v error -y -f rawvideo -pixel_format rgba -video_size " +
                              std::to_string(width) + "x" + std::to_string(height) +
                              " -framerate 60 -i - " + (flip ? "-vf vflip " : "") +
                              "-an -c:v libx264 -preset veryfast -crf 18 -pix_fmt yuv420p logs/" +
                              name + ".mp4";
#ifdef _WIN32
        pipe = _popen(command.c_str(), "wb");
#else
        pipe = popen(command.c_str(), "w");
#endif
        require(pipe, "open video encoder");
    }
    void write(const std::vector<uint8_t> &image) {
        require(pipe && std::fwrite(image.data(), 1, image.size(), pipe) == image.size(),
                "write every video frame");
    }
    void close() {
        if (!pipe)
            return;
#ifdef _WIN32
        int result = _pclose(pipe);
#else
        int result = pclose(pipe);
#endif
        pipe = nullptr;
        require(result == 0, "video encoder finished successfully");
    }
};
inline int run(GBContext *ctx, const char *mode, int limit) {
    bool menu_only = !std::strcmp(mode, "menu"), new_game = !std::strcmp(mode, "new");
    require(menu_only || new_game || !std::strcmp(mode, "continue"), "mode menu/new/continue");
    require(limit > 0, "positive frame limit");
    if (new_game)
        require(!title_qa::battery_loaded, "New Game audit starts without a battery save");
    if (!menu_only && !new_game)
        require(title_qa::battery_loaded, "Continue requires a valid private battery input");
    FILE *trace = std::fopen("logs/boot.csv", "w");
    require(trace, "private logs directory exists");
    std::fputs("frame,cycles,phase,active,blend,target,progress,bgp,lcdc,map,input,native_errors,"
               "pc,sp,bank,root\n",
               trace);
    const bool video = std::getenv("BOOT_VIDEO") != nullptr;
    Movie composed, original;
    std::vector<uint8_t> previous;
    const uint32_t started = SDL_GetTicks();
    int first_title = -1, first_menu = -1, first_world = -1, blends = 0, native_frames = 0;
    int stable_menu = 0, world_frames = 0, count = 0;
    const char *last_input = nullptr;
    bool finished = false, title_fade_finished = false;
    auto last_phase = title_state::Phase::None;
    for (int frame = 0; frame < limit; ++frame) {
        const char *input = nullptr;
        // Leave copyright, Game Freak and the entire Pikachu intro untouched.
        if (first_title >= 0 && frame >= first_title + 240 && frame < first_title + 252)
            input = "START";
        if (!menu_only && first_menu >= 0 && frame >= first_menu + 90 &&
            (frame - first_menu - 90) % 60 < 12 && first_world < 0)
            input = "A";
        if (input != last_input) {
            std::string script = std::string("c0:") + (input ? input : "") + ":4294967296";
            gb_platform_set_input_script(input ? script.c_str() : nullptr);
            last_input = input;
        }
        if (video) {
            int64_t remaining = int64_t(frame * 1000 / 60) - uint32_t(SDL_GetTicks() - started);
            if (remaining > 0)
                SDL_Delay(uint32_t(remaining));
        }
        gb_reset_frame(ctx);
        ctx->stopped = 0;
        unsigned slices = 0;
        while (!ctx->frame_done) {
            require(gb_platform_poll_events(ctx) && ++slices < 1000,
                    "bounded original engine frame");
            gb_run_cycles(ctx, 70224);
        }
        ReadOnlyMemory memory{ctx};
        gb_platform_render_frame(gb_get_framebuffer(ctx));
        require(memory.unchanged(ctx), "presentation leaves WRAM/VRAM/ERAM/framebuffer untouched");
        require(glGetError() == GL_NO_ERROR && pallet3d_input_mask() == 255,
                "OpenGL and neutral relative controls");
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        int width = viewport[2], height = viewport[3];
        std::vector<uint8_t> image(size_t(width) * height * 4);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, image.data());
        auto phase = title_state::sample(ctx);
        auto blend = pallet3d_blend();
        const auto *lcd = gb_get_framebuffer(ctx);
        if (phase == title_state::Phase::None && first_world < 0 &&
            (last_phase == title_state::Phase::Continue ||
             last_phase == title_state::Phase::NewGame)) {
            // A native fallback while the summary is still visible would
            // crossfade its framed panel into a second, enlarged copy.
            require(std::all_of(lcd, lcd + 160 * 144,
                                [](uint32_t pixel) { return (pixel & 0xffffff) == 0xffffff; }),
                    "title menu exits only after the original LCD has faded to white");
        }
        int native_errors = 0;
        if (first_title < 0 && phase == title_state::Phase::None) {
            require(!pallet3d_active() && !blend.active && !input,
                    "unmodified native intro lifetime");
            int scale = std::max(1, std::min(width / 160, height / 144));
            int left = (width - 160 * scale) / 2, top = (height - 144 * scale) / 2;
            for (int y = 0; y < 144; ++y)
                for (int x = 0; x < 160; ++x) {
                    int p = ((height - 1 - (top + y * scale + scale / 2)) * width + left +
                             x * scale + scale / 2) *
                            4;
                    uint32_t color = lcd[y * 160 + x];
                    native_errors += image[p] != uint8_t(color >> 16) ||
                                     image[p + 1] != uint8_t(color >> 8) ||
                                     image[p + 2] != uint8_t(color);
                }
            require(native_errors == 0, "native intro pixels match the original LCD");
            ++native_frames;
        }
        if (phase == title_state::Phase::Title) {
            if (first_title < 0) {
                first_title = frame;
                require(native_frames > 1500 && !previous.empty(),
                        "complete unskipped intro precedes title");
                require(blend.active && blend.target_3d && blend.progress == 0,
                        "title starts with the existing complete-frame fade");
                require(presentation_qa::differences(previous, image) == 0,
                        "first title frame preserves the completed intro frame");
            }
            if (blend.active) {
                require(blend.target_3d && pallet3d_active(), "title fade owns the frame");
                ++blends;
            } else if (first_title >= 0)
                title_fade_finished = true;
        }
        if (phase != title_state::Phase::None)
            require(pallet3d_active(), "recognized title/menu phase is presented");
        if (phase == title_state::Phase::Menu) {
            if (first_menu < 0)
                first_menu = frame;
            ++stable_menu;
        } else
            stable_menu = 0;
        if (phase == title_state::Phase::None && pallet::view(ctx) == pallet::View::Overworld) {
            require(pallet3d_active(), "the reached world is presented in 3D");
            if (first_world < 0)
                first_world = frame;
            ++world_frames;
        } else
            world_frames = 0;
        if (video) {
            if (!composed.pipe) {
                composed.open(width, height, "boot-composed", true);
                original.open(160, 144, "boot-original", false);
            }
            composed.write(image);
            std::vector<uint8_t> raw(160 * 144 * 4);
            for (int p = 0; p < 160 * 144; ++p) {
                raw[p * 4] = uint8_t(lcd[p] >> 16);
                raw[p * 4 + 1] = uint8_t(lcd[p] >> 8);
                raw[p * 4 + 2] = uint8_t(lcd[p]);
                raw[p * 4 + 3] = 255;
            }
            original.write(raw);
        }
        std::fprintf(trace, "%d,%llu,%d,%d,%d,%d,%.6f,%u,%u,%u,%s,%d,%04x,%04x,%02x,%04x\n", frame,
                     (unsigned long long)ctx->cycles, int(phase), pallet3d_active(), blend.active,
                     blend.target_3d, blend.progress, ctx->io[0x47], ctx->io[0x40],
                     pallet::read(ctx, pallet::Map), input ? input : "", native_errors, ctx->pc,
                     ctx->sp, ctx->hram[0x38], ctx->wram[0x1ffd] | (ctx->wram[0x1ffe] << 8));
        if (frame % 120 == 0 || phase != last_phase ||
            (first_title >= 0 && frame <= first_title + 30) || frame == first_world) {
            char path[96];
            std::snprintf(path, sizeof(path), "logs/boot-%04d.ppm", frame);
            capture_surface(path);
        }
        last_phase = phase;
        previous = std::move(image);
        count = frame + 1;
        if ((menu_only && stable_menu >= 90) || (!menu_only && world_frames >= 60)) {
            finished = true;
            capture_surface("logs/boot-final.ppm");
            break;
        }
    }
    composed.close();
    original.close();
    std::fclose(trace);
    require(finished && first_menu >= 0 && title_fade_finished && blends >= 3,
            "bounded boot reaches menu/world through visible title fade");
    require(gb_context_save_state_file(ctx, "logs/boot-final.state"), "private final state");
    std::fprintf(stderr,
                 "[BOOT] PASS mode=%s frames=%d native=%d title=%d menu=%d world=%d "
                 "blend_frames=%d video=%d\n",
                 mode, count, native_frames, first_title, first_menu, first_world, blends, video);
    return 0;
}
} // namespace boot_qa
