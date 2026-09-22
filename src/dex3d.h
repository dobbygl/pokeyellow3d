#pragma once
#include "dex_menu_state.h"
// Included after pallet3d's GL primitives. Owns a texture and transient device
// geometry; the world camera, actor buffers and resident map meshes are kept.
namespace dex3d {
GLuint texture = 0;
int species = 0;
mon_pic::Cache cache;
dex_state::Selection selection;
bool list_mode = false;
mon_pic::Picture picture;
battle::Image image{};
std::array<uint32_t, 160 * 144> lcd{};
bool lcd_valid = false, presented = false;
size_t uploads = 0;
size_t font_generation = 0;
PalletDexMenuInfo menu_shown{};
void shutdown() {
    if (texture)
        glDeleteTextures(1, &texture);
    texture = 0;
    species = 0;
    picture = {};
    image = {};
    lcd_valid = presented = false;
    uploads = 0;
    font_generation = 0;
    cache = {};
    selection = {};
    list_mode = false;
    menu_shown = {};
}
bool initialize_texture() {
    if (texture)
        return true;
    std::vector<uint8_t> data(AW * AH * 4);
    for (int i = 0; i < 4; i++)
        data[((AH - 1) * AW + AW - 1) * 4 + i] = 255;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, AW, AH, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    return texture != 0;
}
void panel(std::vector<Vertex> &v, float left, float top, float w, float h, Color color,
           UV uv = Solid, float z = .43f) {
    quad(v, {left, top, z}, {left + w, top, z}, {left + w, top - h, z}, {left, top - h, z}, color,
         uv);
}
void lcd_region(std::vector<Vertex> &v, const GBContext *ctx, lcd_overlay::Rect r, float left,
                float top, float scale) {
    const auto &font = rom_font::get(ctx->rom, ctx->rom_size);
    if (menu_style == ui_preferences::Style::Integrated &&
        rom_text_region::ready(ctx->wram + 0x3a0, ctx->vram, font, lcd.data(),
                               {r.x, r.y, r.w, r.h})) {
        panel(v, left, top, r.w * 8 * scale, r.h * 8 * scale, ui_theme::PanelColor);
        for (int y = 0; y < r.h; ++y)
            for (int x = 0; x < r.w; ++x) {
                int glyph = ctx->wram[0x3a0 + (r.y + y) * 20 + r.x + x] - 0x80;
                if (glyph < 0)
                    continue;
                panel(
                    v, left + x * 8 * scale, top - y * 8 * scale, 8 * scale, 8 * scale,
                    ui_theme::InkColor,
                    {float(glyph % 16 * 8) - .25f, float(256 + glyph / 16 * 8) - .25f, 8.5f, 8.5f},
                    .44f);
            }
        return;
    }
    panel(v, left, top, r.w * 8 * scale, r.h * 8 * scale, White,
          {float(r.x * 8), float(64 + r.y * 8), float(r.w * 8), float(r.h * 8)});
}
void list_text(GBContext *ctx, int w, int h, const firstperson::Matrix &matrix) {
    menu_shown.active = true;
    menu_text::prime();
    const auto &font = rom_font::get(ctx->rom, ctx->rom_size);
    auto snapshot = dex_menu::sample(ctx, font);
    auto desired = dex_state::list(ctx);
    if (!snapshot.ready || !desired.number || desired.number != selection.number ||
        desired.row != selection.row ||
        !dex_state::visible(ctx, gb_get_framebuffer(ctx), desired)) {
        lcd_overlay::framed(gb_get_framebuffer(ctx), float(w), float(h));
        return;
    }
    auto plan =
        menu_text::prepare(snapshot.text.data(), ctx->vram, font, snapshot.layout, float(w),
                           float(h), 4, 2, 2, menu_text::Placement::Full, ctx->rom, ctx->rom_size);
    auto project = [&](float x, float y) {
        float clip_w = matrix[3] * x + matrix[7] * y + matrix[11] * .44f + matrix[15];
        float px = (matrix[0] * x + matrix[4] * y + matrix[8] * .44f + matrix[12]) / clip_w;
        float py = (matrix[1] * x + matrix[5] * y + matrix[9] * .44f + matrix[13]) / clip_w;
        return ImVec2{(px + 1) * w * .5f, (1 - py) * h * .5f};
    };
    constexpr float positions[][3]{
        {-5.68f, 3.15f, .039f}, {4.40f, 1.68f, .043f}, {2.f, 3.57f, .049f}, {4.05f, 3.57f, .049f}};
    for (size_t i = 0; i < plan.count; ++i) {
        auto r = dex_menu::Regions[i];
        auto &p = plan.panels[i];
        auto a = project(positions[i][0], positions[i][1]);
        auto b = project(positions[i][0] + r.w * 8 * positions[i][2],
                         positions[i][1] - r.h * 8 * positions[i][2]);
        int scale = int(std::floor(std::min((b.x - a.x) / (r.w * 8), (b.y - a.y) / (r.h * 8))));
        if (scale < 1 || a.x < 0 || b.x > w || a.y < 0 || b.y > h) {
            lcd_overlay::framed(gb_get_framebuffer(ctx), float(w), float(h));
            return;
        }
        int left = int(std::floor((a.x + b.x - r.w * 8 * scale) / 2));
        int top = int(std::floor((a.y + b.y - r.h * 8 * scale) / 2));
        menu_shown.left[i] = left;
        menu_shown.top[i] = top;
        menu_shown.scale[i] = scale;
        p.scale = scale;
        p.origin_x = float(left - r.x * 8 * scale);
        p.origin_y = float(top - r.y * 8 * scale);
        p.left = float(left - 4);
        p.top = float(top - 4);
        p.right = float(left + r.w * 8 * scale + 4);
        p.bottom = float(top + r.h * 8 * scale + 4);
    }
    auto drawn = menu_text::regions(snapshot.text.data(), ctx->vram, ctx->rom, ctx->rom_size,
                                    gb_get_framebuffer(ctx), snapshot.layout, float(w), float(h),
                                    menu_text::Placement::Full, &plan);
    if (!drawn.panels)
        return;
    menu_shown.fallback = false;
    // All validation precedes emission. Draw the verified caught pictogram with
    // its four original indices; it is never treated as a font character.
    auto *dl = ImGui::GetForegroundDrawList();
    const auto &p = plan.panels[0];
    int cell = p.scale * 8;
    for (int row = 0; row < 7; ++row)
        if (snapshot.caught[size_t(row)]) {
            ImVec2 from{p.origin_x + 3 * cell, p.origin_y + (3 + row * 2) * cell};
            dl->AddImage((ImTextureID)(intptr_t)menu_text::graphics_texture, from,
                         {from.x + cell, from.y + cell}, {.5f, 0}, {1, 1}, ui_theme::White);
        }
}
bool draw(GBContext *ctx, int w, int h, bool) {
    presented = false;
    auto desired = dex_state::list(ctx);
    bool was_list = list_mode;
    list_mode = desired.number != 0;
    const uint32_t *original = gb_get_framebuffer(ctx);
    if (list_mode) {
        // WRAM scroll/cursor changes precede the rendered LCD by up to three
        // frames. Switch only when its original number and cursor are visible.
        // Keep the preceding portrait only while its own cursor is still drawn;
        // an incomplete transfer gets an empty screen, never stale artwork.
        if (dex_state::visible(ctx, original, desired))
            selection = desired;
        else if (!was_list || !dex_state::visible(ctx, original, selection))
            selection = {};
        selection.seen = desired.seen;
        selection.caught = desired.caught;
        selection.registration = dex_state::flag(ctx, dex_state::Owned, selection.number)
                                     ? dex_state::Registration::Caught
                                 : dex_state::flag(ctx, dex_state::Seen, selection.number)
                                     ? dex_state::Registration::Seen
                                     : dex_state::Registration::Absent;
    } else
        selection = {};
    if (!list_mode && !dex_state::data(ctx))
        return false;
    int current = list_mode ? selection.species : battle::read(ctx, dex_state::Current);
    species = current;
    picture = cache.get(ctx->rom, ctx->rom_size, current, true);
    // Never show a cached previous species or an unverified partial transfer.
    if ((!picture.valid && (!list_mode || selection.number)) ||
        (!list_mode && std::memcmp(picture.tiles.data(), ctx->vram + 0x1000, mon_pic::TileBytes)))
        return false;
    if (!initialize_texture())
        return false;
    glBindTexture(GL_TEXTURE_2D, texture);
    if (menu_style == ui_preferences::Style::Integrated) {
        const auto &font = rom_font::get(ctx->rom, ctx->rom_size);
        if (font.valid && font_generation != rom_font::decodes) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 256, rom_font::Width, rom_font::Height, GL_RGBA,
                            GL_UNSIGNED_BYTE, font.rgba.data());
            font_generation = rom_font::decodes;
        }
    }
    auto next = mon_pic::rgba(picture, battle::palette(ctx, current),
                              list_mode && selection.registration == dex_state::Registration::Seen);
    if (list_mode && selection.registration == dex_state::Registration::Absent)
        next = {};
    if (list_mode && selection.number)
        for (int offset = -2; offset <= 2; offset++) {
            int n = selection.number + offset;
            if (dex_state::flag(ctx, dex_state::Seen, n))
                cache.get(ctx->rom, ctx->rom_size, mon_pic::species(ctx->rom, ctx->rom_size, n),
                          true);
        }
    if (next != image) {
        image = next;
        glTexSubImage2D(GL_TEXTURE_2D, 0, 192, 0, 56, 56, GL_RGBA, GL_UNSIGNED_BYTE, image.data());
        ++uploads;
    }
    if (!lcd_valid || std::memcmp(lcd.data(), original, sizeof(lcd))) {
        std::copy_n(original, lcd.size(), lcd.begin());
        lcd_valid = true;
        std::array<uint8_t, 160 * 144 * 4> rgba{};
        for (size_t i = 0; i < lcd.size(); i++) {
            rgba[i * 4] = uint8_t(lcd[i] >> 16);
            rgba[i * 4 + 1] = uint8_t(lcd[i] >> 8);
            rgba[i * 4 + 2] = uint8_t(lcd[i]);
            rgba[i * 4 + 3] = 255;
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 64, 160, 144, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        ++uploads;
    }
    std::vector<Vertex> v;
    box(v, -6.8f, -.35f, 6.65f, .7f, -3.5f, 4, ui_theme::DexLeft);
    box(v, .15f, -.35f, 6.65f, .7f, -3.5f, 4, ui_theme::DexRight);
    box(v, -.15f, -.22f, .3f, .65f, -3.2f, 3.7f, ui_theme::DexHinge);
    panel(v, -6.32f, 3.48f, 5.7f, 6.45f, ui_theme::DexBezel, Solid, .38f);
    panel(v, .63f, 2.8f, 5.6f, 5.65f, list_mode ? ui_theme::DexList : ui_theme::DexScreen, Solid,
          .38f);
    panel(v, -6.12f, 3.25f, 5.3f, 5.95f, White, Solid, .40f);
    // Original number/name/category/height/weight and description pixels. No
    // typeface or menu content is reconstructed; the source portrait is moved
    // to the other screen rather than duplicated with the text.
    if (list_mode) {
        if (menu_style != ui_preferences::Style::Integrated)
            lcd_region(v, ctx, {0, 0, 14, 18}, -5.68f, 3.15f, .039f);
        else
            panel(v, -6.12f, 3.25f, 5.3f, 5.95f, ui_theme::PanelColor, Solid, .41f);
        panel(v, .82f, 2.02f, 3.62f, 3.62f, White, {192, 0, 56, 56});
        // The counters on the body are also the game's own glyphs, driven by
        // the same seen/owned bitmaps exposed in Selection, never re-typeset.
        if (menu_style != ui_preferences::Style::Integrated) {
            lcd_region(v, ctx, {15, 8, 5, 9}, 4.48f, 1.68f, .039f);
            lcd_region(v, ctx, {16, 1, 4, 2}, 2.0f, 3.57f, .049f);
            lcd_region(v, ctx, {16, 4, 4, 2}, 4.05f, 3.57f, .049f);
        }
    } else {
        lcd_region(v, ctx, {2, 8, 5, 1}, -5.85f, 3.1f, .041f);
        lcd_region(v, ctx, {9, 2, 10, 7}, -5.5f, 2.55f, .052f);
        lcd_region(v, ctx, {1, 10, 18, 7}, -6.1f, -.52f, .0365f);
        panel(v, .82f, 2.65f, 5.2f, 5.2f, White, {192, 0, 56, 56});
    }
    box(v, .7f, .35f, .55f, .18f, 3.18f, 3.73f, ui_theme::DexBlueLed);
    if (!list_mode) {
        box(v, 1.6f, .35f, .22f, .13f, 3.32f, 3.54f, ui_theme::DexGreenLed);
        box(v, 2.1f, .35f, .22f, .13f, 3.32f, 3.54f, ui_theme::DexAmberLed);
        for (int i = 0; i < 4; i++)
            panel(v, 4.15f + i * .32f, 3.53f, .14f, .48f, ui_theme::DexGrille);
    }
    firstperson::Camera camera;
    float distance = std::max(6.7f, 11.2f / (float(w) / h));
    camera.x = -distance * .105f;
    camera.y = .25f;
    camera.z = distance;
    camera.yaw = .105f;
    auto matrix = camera.perspective(float(w) / h);
    glViewport(0, 0, w, h);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(ui_theme::DexBackground.r, ui_theme::DexBackground.g, ui_theme::DexBackground.b,
                 1);
    glDepthMask(GL_TRUE);
    glClearDepthf(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program);
    glUniformMatrix4fv(matrix_loc, 1, GL_FALSE, matrix.data());
    glUniform2f(fade_loc, 1, 0);
    glUniform1f(fog_loc, 0);
    glUniform1f(sky_loc, 0);
    glUniform1f(xray_loc, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(program, "image"), 0);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Vertex), v.data(), GL_STREAM_DRAW);
    for (int i = 0; i < 3; i++)
        glEnableVertexAttribArray(i);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, p));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, u));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, c));
    glDrawArrays(GL_TRIANGLES, 0, GLsizei(v.size()));
    for (int i = 0; i < 3; i++)
        glDisableVertexAttribArray(i);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
    if (list_mode && menu_style == ui_preferences::Style::Integrated)
        list_text(ctx, w, h, matrix);
    presented = true;
    return true;
}
} // namespace dex3d
