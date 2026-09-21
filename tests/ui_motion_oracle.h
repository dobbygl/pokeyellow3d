#pragma once
#include "menu_motion.h"
#include "pallet3d.h"
#include <SDL_opengles2.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

// Private opt-in evidence. The engine stream contains every byte of every
// canonical savestate, framed by its length; the external runner compresses it
// through a FIFO and compares the decompressed streams without hash shortcuts.
namespace ui_motion_qa {
inline bool enabled = false, capture = false, was_transition = false;
inline FILE *engine = nullptr, *frames_file = nullptr, *panels_file = nullptr;
inline size_t frames = 0, opening = 0, closing = 0, paused = 0, captures = 0;
inline void require(bool ok, const char *message) {
    if (!ok) {
        std::fprintf(stderr, "[UI-MOTION] FAIL: %s\n", message);
        std::exit(64);
    }
}
inline void report() {
    if (!enabled)
        return;
    if (engine)
        std::fclose(engine);
    if (frames_file)
        std::fclose(frames_file);
    if (panels_file)
        std::fclose(panels_file);
    engine = frames_file = panels_file = nullptr;
    std::fprintf(stderr, "[UI-MOTION] frames=%zu opening=%zu closing=%zu paused=%zu captures=%zu\n",
                 frames, opening, closing, paused, captures);
}
inline void install() {
    const char *engine_path = std::getenv("QA_MENU_ENGINE_TRACE");
    capture = std::getenv("QA_MENU_MOTION_CAPTURE") != nullptr;
    enabled = engine_path || capture || std::getenv("QA_MENU_MOTION_AUDIT");
    if (!enabled)
        return;
    if (engine_path) {
        engine = std::fopen(engine_path, "wb");
        require(engine, "open canonical engine stream");
    }
    frames_file = std::fopen("logs/motion-frames.csv", "w");
    panels_file = std::fopen("logs/motion-panels.csv", "w");
    require(frames_file && panels_file, "open motion evidence");
    std::fprintf(frames_file, "frame,cycles,paused,panels,settings,focused,animated\n");
    std::fprintf(
        panels_file,
        "frame,domain,x,y,w,h,target,visible,amount,opacity,offset,left,top,right,bottom\n");
    std::atexit(report);
}
inline void engine_frame(GBContext *ctx) {
    if (!engine)
        return;
    constexpr const char *path = "logs/motion-current.state";
    require(gb_context_save_state_file(ctx, path), "serialize complete original engine");
    FILE *input = std::fopen(path, "rb");
    require(input && std::fseek(input, 0, SEEK_END) == 0, "open canonical frame");
    long length = std::ftell(input);
    require(length > 0 && length < 16 * 1024 * 1024 && std::fseek(input, 0, SEEK_SET) == 0,
            "bounded canonical frame size");
    uint32_t size = uint32_t(length);
    require(std::fwrite(&size, sizeof(size), 1, engine) == 1, "write frame length");
    std::array<uint8_t, 8192> bytes{};
    while (size) {
        size_t count = std::min(size_t(size), bytes.size());
        require(std::fread(bytes.data(), 1, count, input) == count &&
                    std::fwrite(bytes.data(), 1, count, engine) == count,
                "copy every canonical engine byte");
        size -= uint32_t(count);
    }
    std::fclose(input);
}
inline void observe(GBContext *ctx, int width, int height, bool menu_open) {
    if (!enabled || !ctx)
        return;
    engine_frame(ctx);
    const auto &motion = menu_motion::state;
    std::fprintf(frames_file, "%zu,%llu,%d,%zu,%d,%d,%d\n", frames, (unsigned long long)ctx->cycles,
                 int(motion.paused), motion.panels.size(), int(menu_open),
                 int(pallet3d_window_focused()), int(menu_motion::enabled));
    bool transition = false;
    paused += motion.paused;
    for (const auto &panel : motion.panels) {
        const auto &r = panel.key.region;
        const auto &b = panel.bounds;
        std::fprintf(panels_file, "%zu,%d,%d,%d,%d,%d,%d,%d,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n",
                     frames, panel.key.placement, r.x, r.y, r.w, r.h, int(panel.target),
                     int(panel.visible), double(panel.amount), double(panel.opacity()),
                     double(panel.offset()), double(b.left), double(b.top), double(b.right),
                     double(b.bottom));
        transition |= panel.visible && (panel.amount < 1 || !panel.target);
        opening += panel.visible && panel.target && panel.amount < 1;
        closing += panel.visible && !panel.target;
    }
    if (capture && captures < 96 && (transition || was_transition)) {
        std::vector<uint8_t> pixels(size_t(width) * height * 4);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        char path[96];
        std::snprintf(path, sizeof(path), "logs/motion-%06zu.ppm", frames);
        FILE *output = std::fopen(path, "wb");
        require(output, "open transition capture");
        std::fprintf(output, "P6\n%d %d\n255\n", width, height);
        for (int y = height - 1; y >= 0; --y)
            for (int x = 0; x < width; ++x)
                require(std::fwrite(pixels.data() + (size_t(y) * width + x) * 4, 1, 3, output) == 3,
                        "write transition capture");
        std::fclose(output);
        ++captures;
    }
    was_transition = transition;
    ++frames;
}
} // namespace ui_motion_qa
