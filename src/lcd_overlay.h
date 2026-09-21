#pragma once
#include "imgui.h"
#include "ui_theme.h"
#include <SDL_opengles2.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include "menu_layout.h"

// The only LCD upload/composition path used by the custom scenes. Regions are
// expressed in Game Boy tiles; no text, menu input or machine state is changed.
namespace lcd_overlay {
struct Rect {
    int x, y, w, h;
};
constexpr Rect Bottom{0, 12, 20, 6}, Full{0, 0, 20, 18};
inline GLuint texture = 0;
inline std::array<uint32_t, 160 * 144> cached{};
inline std::array<bool, 160 * 144> valid{};
inline size_t uploads = 0, bytes = 0;

inline void shutdown() {
    if (texture)
        glDeleteTextures(1, &texture);
    texture = 0;
    valid.fill(false);
    uploads = bytes = 0;
}
inline bool upload(const uint32_t *framebuffer, Rect r) {
    if (!framebuffer || r.x < 0 || r.y < 0 || r.w <= 0 || r.h <= 0 || r.x + r.w > 20 ||
        r.y + r.h > 18)
        return false;
    if (!texture) {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 160, 144, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        valid.fill(false);
    }
    bool changed = false;
    for (int y = r.y * 8; y < (r.y + r.h) * 8; y++)
        for (int x = r.x * 8; x < (r.x + r.w) * 8; x++) {
            int i = y * 160 + x;
            changed |= !valid[i] || cached[i] != framebuffer[i];
        }
    if (changed) {
        std::vector<uint8_t> rgba(size_t(r.w) * r.h * 64 * 4);
        size_t p = 0;
        for (int y = r.y * 8; y < (r.y + r.h) * 8; y++)
            for (int x = r.x * 8; x < (r.x + r.w) * 8; x++) {
                int i = y * 160 + x;
                uint32_t c = framebuffer[i];
                cached[i] = c;
                valid[i] = true;
                rgba[p++] = uint8_t(c >> 16);
                rgba[p++] = uint8_t(c >> 8);
                rgba[p++] = uint8_t(c);
                rgba[p++] = 255;
            }
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, r.x * 8, r.y * 8, r.w * 8, r.h * 8, GL_RGBA,
                        GL_UNSIGNED_BYTE, rgba.data());
        ++uploads;
        bytes += rgba.size();
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture != 0;
}
inline void draw(const uint32_t *framebuffer, Rect r, float w, float h, float alpha = 1,
                 bool bottom_aligned = false) {
    if (!upload(framebuffer, r))
        return;
    float scale = std::max(1.f, std::floor(std::min(w / 160, h / 144)));
    float left = (w - 160 * scale) * .5f, top = (h - 144 * scale) * (bottom_aligned ? 1.f : .5f);
    ImGui::GetForegroundDrawList()->AddImage(
        (ImTextureID)(intptr_t)texture, {left + r.x * 8 * scale, top + r.y * 8 * scale},
        {left + (r.x + r.w) * 8 * scale, top + (r.y + r.h) * 8 * scale}, {r.x / 20.f, r.y / 18.f},
        {(r.x + r.w) / 20.f, (r.y + r.h) / 18.f}, ui_theme::opacity(ui_theme::White, alpha));
}
inline int framed_scale(float w, float h) {
    // Leave enough of small interiors visible to establish the retained scene.
    // At the default 800x720 window the original LCD remains crisp at 3x.
    return std::max(1, int(std::floor(std::min(w * .75f / 160, h * .75f / 144))));
}
inline void framed(const uint32_t *framebuffer, float w, float h, float alpha = 1) {
    if (!upload(framebuffer, Full))
        return;
    int scale = framed_scale(w, h);
    float left = std::floor((w - 160 * scale) * .5f), top = std::floor((h - 144 * scale) * .5f);
    auto *dl = ImGui::GetForegroundDrawList();
    alpha = std::clamp(alpha, 0.f, 1.f);
    dl->AddRectFilled({left - 5, top - 5}, {left + 160 * scale + 5, top + 144 * scale + 5},
                      ui_theme::opacity(ui_theme::Frame, alpha));
    dl->AddRect({left - 2, top - 2}, {left + 160 * scale + 2, top + 144 * scale + 2},
                ui_theme::opacity(ui_theme::FrameEdge, alpha));
    dl->AddImage((ImTextureID)(intptr_t)texture, {left, top},
                 {left + 160 * scale, top + 144 * scale}, {0, 0}, {1, 1},
                 ui_theme::opacity(ui_theme::White, alpha));
}
inline void regions(const uint32_t *framebuffer, const menu_layout::Layout &layout, float w,
                    float h) {
    float scale = std::max(1.f, std::floor(std::min(w / 160, h / 144)));
    float left = (w - 160 * scale) * .5f, top = h - 144 * scale;
    auto *dl = ImGui::GetForegroundDrawList();
    // All shadows go behind all images, including overlapping original boxes.
    for (size_t i = 0; i < layout.count; i++) {
        auto r = layout.regions[i];
        float x = left + r.x * 8 * scale, y = top + r.y * 8 * scale;
        for (int pad = 3; pad >= 1; pad--)
            dl->AddRectFilled({x - pad, y - pad + 2},
                              {x + r.w * 8 * scale + pad, y + r.h * 8 * scale + pad + 2},
                              ui_theme::Shadow);
    }
    for (size_t i = 0; i < layout.count; i++) {
        auto r = layout.regions[i];
        draw(framebuffer, {r.x, r.y, r.w, r.h}, w, h, 1, true);
    }
}
} // namespace lcd_overlay
