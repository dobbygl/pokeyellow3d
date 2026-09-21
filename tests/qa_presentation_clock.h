#pragma once
#include "gb_presentation.h"
#include "imgui.h"
#include "pallet3d.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "ui_style_oracle.h"

#ifdef QA_DETERMINISTIC_SDL_CLOCK
void qa_advance_sdl_clock(float seconds);
#endif

// Only the smoke executable includes this adapter. The playable application
// keeps SDL/ImGui's real-time clock and the production presentation callbacks.
// Frame stepping alone does not fix camera easing: host rendering cost would
// otherwise change the accumulated presentation time at a capture waypoint.
namespace qa_clock {
inline bool (*original_frame)(GBContext *, int, int, bool) = nullptr;
inline bool (*original_begin)(GBContext *, bool, const uint32_t *) = nullptr;
inline void (*original_before_swap)(int, int, bool) = nullptr;
inline GBContext *context = nullptr;
inline float seconds = 1.f / 60.f;
inline bool fixed = true;
inline void before_swap(int width, int height, bool menu_open) {
    original_before_swap(width, height, menu_open);
    bool visible = SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE;
    bool focused = pallet3d_window_focused();
    bool backend_may_change = !(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange);
    if (visible != (menu_open || !focused) || backend_may_change != visible) {
        std::fprintf(stderr, "[QA-CURSOR] FAIL menu=%d focus=%d visible=%d\n", menu_open, focused,
                     visible);
        std::exit(63);
    }
    ui_style_qa::observe(context, width, height, menu_open);
}
inline bool begin(GBContext *ctx, bool menu_open, const uint32_t *framebuffer) {
#ifdef QA_DETERMINISTIC_SDL_CLOCK
    if (fixed)
        qa_advance_sdl_clock(seconds);
#endif
    return original_begin(ctx, menu_open, framebuffer);
}
inline bool frame(GBContext *ctx, int width, int height, bool menu_open) {
    context = ctx;
    if (fixed)
        ImGui::GetIO().DeltaTime = seconds;
    return original_frame(ctx, width, height, menu_open);
}
inline bool install() {
    const char *style = std::getenv("QA_MENU_STYLE");
    if (style && std::strcmp(style, "classic") && std::strcmp(style, "integrated")) {
        std::fprintf(stderr, "QA_MENU_STYLE must be classic or integrated\n");
        return false;
    }
    pallet3d_menu_style(style && !std::strcmp(style, "integrated")
                            ? ui_preferences::Style::Integrated
                            : ui_preferences::Style::Classic);
    ui_style_qa::enabled = std::getenv("QA_GLYPH_ORACLE") != nullptr;
    std::atexit(ui_style_qa::report);
    if (const char *value = std::getenv("QA_FRAME_SECONDS")) {
        if (!std::strcmp(value, "wall"))
            fixed = false;
        else {
            char *end = nullptr;
            seconds = std::strtof(value, &end);
            if (!end || end == value || *end || !std::isfinite(seconds) || seconds <= 0 ||
                seconds > .1f) {
                std::fprintf(stderr, "QA_FRAME_SECONDS must be wall or a number in (0, 0.1]\n");
                return false;
            }
        }
    }
    auto hooks = pallet3d_presentation_hooks();
    original_before_swap = hooks.before_swap;
    hooks.before_swap = before_swap;
    original_begin = hooks.begin_frame;
    hooks.begin_frame = begin;
    original_frame = hooks.frame;
    hooks.frame = frame;
    std::fprintf(stderr, "[QA-CLOCK] presentation=%s seconds=%.9g; guest clock unchanged\n",
                 fixed ? "fixed" : "wall", seconds);
    return gb_platform_set_presentation(&hooks);
}
inline void capture_state(const char *image_path) {
    if (context && std::getenv("QA_CAPTURE_STATES")) {
        std::string path = std::string(image_path) + ".machine";
        if (!gb_context_save_state_file(context, path.c_str())) {
            std::fprintf(stderr, "Failed to capture private machine state: %s\n", path.c_str());
            std::exit(19);
        }
    }
}
} // namespace qa_clock
