#pragma once
// Included after the world drawing primitives. The title reads ROM catalog
// geometry and live LCD/VRAM; its camera never feeds the emulated machine.
namespace title3d {
inline bool presented = false, clock_ready = false;
inline uint64_t last_cycles = 0;
inline double seconds = 0;
inline GLuint lettering = 0;
inline void reset() {
    presented = clock_ready = false;
    last_cycles = 0;
    seconds = 0;
}
inline void shutdown() {
    if (lettering)
        glDeleteTextures(1, &lettering);
    lettering = 0;
    reset();
}
inline firstperson::Matrix camera(float aspect, Vec &position) {
    double phase = seconds * (2 * firstperson::Pi / 80);
    // Keep the portrait inside the title's fixed logo/copyright margins for
    // the entire loop, including the near end of the travelling path.
    position = {10 + float(std::sin(phase)) * 1.25f, 4.8f, 22 + float(std::cos(phase)) * .25f};
    const float ex = position.x, ey = position.y, ez = position.z;
    const float s = .22f, c = std::sqrt(1 - s * s);
    const float f = 1 / std::tan(65.f * firstperson::Pi / 360);
    constexpr float near = .1f, far = 96, a = (far + near) / (near - far),
                    b = 2 * near * far / (near - far);
    return {f / aspect,
            0,
            0,
            0,
            0,
            f * c,
            a * s,
            -s,
            0,
            -f * s,
            a * c,
            -c,
            -f * ex / aspect,
            f * (-c * ey + s * ez),
            -a * (s * ey + c * ez) + b,
            s * ey + c * ez};
}
inline void original_lettering(const GBContext *ctx, const uint32_t *lcd, int w, int h) {
    if (!lettering) {
        glGenTextures(1, &lettering);
        glBindTexture(GL_TEXTURE_2D, lettering);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    std::array<uint8_t, 160 * 144 * 4> data{};
    const auto &font = rom_font::get(ctx->rom, ctx->rom_size);
    bool shared_caption =
        menu_style == ui_preferences::Style::Integrated &&
        rom_text_region::ready(ctx->wram + 0x3a0, ctx->vram, font, lcd, {0, 17, 20, 1});
    uint32_t background = lcd[0] & 0xffffff;
    for (int y = 0; y < 144; ++y)
        for (int x = 0; x < 160; ++x) {
            int p = y * 160 + x;
            uint32_t color = lcd[p] & 0xffffff;
            data[p * 4] = uint8_t(color >> 16);
            data[p * 4 + 1] = uint8_t(color >> 8);
            data[p * 4 + 2] = uint8_t(color);
            data[p * 4 + 3] = color != background && (y < 64 || y >= 136) ? 255 : 0;
            if (shared_caption && y >= 136) {
                uint8_t tile = ctx->wram[0x3a0 + 17 * 20 + x / 8];
                int glyph = tile - 0x80;
                // Reuse the shared atlas verbatim. Title logos and special
                // copyright tiles outside that atlas keep their original art.
                size_t at = (size_t(glyph >= 0 ? glyph / 16 * 8 + y % 8 : 0) * rom_font::Width +
                             (glyph >= 0 ? glyph % 16 * 8 + x % 8 : 0)) *
                            4;
                data[p * 4] = uint8_t((ui_theme::Ink >> IM_COL32_R_SHIFT) & 0xff);
                data[p * 4 + 1] = uint8_t((ui_theme::Ink >> IM_COL32_G_SHIFT) & 0xff);
                data[p * 4 + 2] = uint8_t((ui_theme::Ink >> IM_COL32_B_SHIFT) & 0xff);
                data[p * 4 + 3] = glyph >= 0 ? font.rgba[at + 3] : 0;
            }
        }
    glBindTexture(GL_TEXTURE_2D, lettering);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 160, 144, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    float scale = std::max(1.f, std::floor(std::min(w / 180.f, h / 160.f)));
    float left = std::floor((w - 160 * scale) * .5f);
    auto *dl = ImGui::GetForegroundDrawList();
    dl->AddImage((ImTextureID)(intptr_t)lettering, {left, 16},
                 {left + 160 * scale, 16 + 64 * scale}, {0, 0}, {1, 64.f / 144});
    dl->AddRectFilled({left - 4, h - 20 - 8 * scale}, {left + 160 * scale + 4, h - 12.f},
                      shared_caption ? ui_theme::Panel : ui_theme::TitleStrip,
                      shared_caption ? ui_theme::Radius : 0);
    dl->AddImage((ImTextureID)(intptr_t)lettering, {left, h - 16 - 8 * scale},
                 {left + 160 * scale, h - 16.f}, {0, 136.f / 144}, {1, 1});
}
inline bool draw(GBContext *ctx, int w, int h, title_state::Phase phase, bool paused) {
    if (!paused && clock_ready && ctx->cycles != last_cycles)
        seconds = std::fmod(seconds + std::clamp(ImGui::GetIO().DeltaTime, 0.f, .1f), 80.0);
    last_cycles = ctx->cycles;
    clock_ready = true;
    if (!presented || atlas_tileset != -1) {
        create_atlas(ctx);
        glBindTexture(GL_TEXTURE_2D, atlas);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, AW, AH, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    }
    presented = true;
    update_meshes(ctx);
    vertices.clear();
    actors_drawn = 0;
    hero_drawn = false;
    world_frame = {};
    world_frame.fp = true;
    world_frame.fog = 1;
    world_frame.dusk = 1;
    world_frame.matrix = camera(float(w) / h, world_frame.eye);
    if (phase == title_state::Phase::Title) {
        auto portrait = title_picture::decode(ctx);
        glBindTexture(GL_TEXTURE_2D, atlas);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 416, title_picture::Width, title_picture::Height,
                        GL_RGBA, GL_UNSIGNED_BYTE, portrait.data());
        // Vertical, camera-facing billboard; the original portrait has a 4:3
        // aspect and includes the title's own animated OAM features.
        constexpr float x = 10, z = 17, half = 1.6f, top = 3.4f, bottom = 1.f;
        float dx = world_frame.eye.x - x, dz = world_frame.eye.z - z;
        float length = std::hypot(dx, dz), rx = dz / length, rz = -dx / length;
        quad(vertices, {x - rx * half, top, z - rz * half}, {x + rx * half, top, z + rz * half},
             {x + rx * half, bottom, z + rz * half}, {x - rx * half, bottom, z - rz * half}, White,
             {0, 416, 96, 72});
    }
    draw_world_frame(w, h, {}, true, false);
    if (phase == title_state::Phase::Title)
        original_lettering(ctx, gb_get_framebuffer(ctx), w, h);
    else {
        if (scene_filter::capture(w, h))
            scene_filter::draw(w, h);
        else
            draw_world_frame(w, h, {.42f, 0}, true, false);
        lcd_overlay::framed(gb_get_framebuffer(ctx), float(w), float(h));
    }
    world_frame = {};
    return true;
}
} // namespace title3d
