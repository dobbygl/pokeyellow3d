#pragma once
// Included beside pc3d after the shared GL primitives. The shelf's geometry,
// atlas and fingerprints are separate from the retained interior.
namespace pc_boxes {
GLuint texture = 0;
mon_pic::Cache portraits;
pc_storage::Snapshot data;
pc_box_state::Selection selected;
std::vector<Vertex> geometry;
size_t rebuilds = 0, uploads = 0;
int width = 0, height = 0;
bool presented = false;
ui_preferences::Style rendered_style = ui_preferences::Style::Classic;
std::array<std::array<float, 4>, 26> slots{}; // screen-space portrait bounds
void reset() {
    data = {};
    selected = {};
    geometry.clear();
    width = height = 0;
    presented = false;
}
void shutdown() {
    if (texture)
        glDeleteTextures(1, &texture);
    texture = 0;
    reset();
    portraits = {};
    rebuilds = uploads = 0;
}
void initialize(const GBContext *ctx) {
    if (texture)
        return;
    std::vector<uint8_t> rgba(AW * AH * 4);
    rom_font::copy_to(ctx->rom, ctx->rom_size, rgba.data(), AW, AH, 0, 256);
    for (int c = 0; c < 4; c++)
        rgba[((AH - 1) * AW + AW - 1) * 4 + c] = 255;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, AW, AH, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
}
float px(float x) {
    return (x - width * .5f) * 18.f / height;
}
float py(float y) {
    return (height * .5f - y) * 18.f / height;
}
void panel(std::vector<Vertex> &v, float x, float y, float w, float h, Color color, UV uv = Solid,
           float z = .015f) {
    // Front-facing labels remain on their own depth plane, but their projected
    // bounds are the requested pixel coordinates rather than slightly enlarged.
    float plane = (9 * 1.428148f - z) / (9 * 1.428148f);
    quad(v, {px(x) * plane, py(y) * plane, z}, {px(x + w) * plane, py(y) * plane, z},
         {px(x + w) * plane, py(y + h) * plane, z}, {px(x) * plane, py(y + h) * plane, z}, color,
         uv);
}
void block(std::vector<Vertex> &v, float x, float y, float w, float h, Color color) {
    box(v, px(x), -.28f, w * 18.f / height, .28f, py(y + h), py(y), color);
}
void text(std::vector<Vertex> &v, const uint8_t *chars, int length, float x, float y, float size,
          Color color = ui_theme::ShelfInk) {
    if (menu_style == ui_preferences::Style::Integrated)
        color = ui_theme::InkColor;
    x = std::round(x);
    y = std::round(y);
    for (int i = 0; i < length && chars[i] != 0x50; i++)
        if (chars[i] >= 0x80) {
            int glyph = chars[i] - 0x80;
            panel(v, x + i * size, y, size, size, color,
                  {float(glyph % 16 * 8) - .25f, float(256 + glyph / 16 * 8) - .25f, 8.5f, 8.5f},
                  .025f);
        }
}
void label(std::vector<Vertex> &v, const char *value, float x, float y, float size,
           Color color = ui_theme::ShelfInk) {
    std::array<uint8_t, 32> chars{};
    size_t n = std::min(std::strlen(value), chars.size());
    for (size_t i = 0; i < n; i++)
        chars[i] = value[i] >= '0' && value[i] <= '9'   ? uint8_t(0xf6 + value[i] - '0')
                   : value[i] >= 'A' && value[i] <= 'Z' ? uint8_t(0x80 + value[i] - 'A')
                                                        : 0x7f;
    text(v, chars.data(), int(n), x, y, size, color);
}
void build(const GBContext *ctx, int w, int h, const pc_storage::Snapshot &next) {
    data = next;
    rendered_style = menu_style;
    width = w;
    height = h;
    geometry.clear();
    slots = {};
    ++rebuilds;
    initialize(ctx);
    glBindTexture(GL_TEXTURE_2D, texture);
    int scale = lcd_overlay::framed_scale(float(w), float(h));
    float left = (w - 160 * scale) / 2.f, top = (h - 144 * scale) / 2.f;
    // The original full LCD keeps its centered integer scale. The open box
    // sits above it, the party below, and the twelve numbered cubbies flank it.
    // This keeps portraits visible while the original lists are being used.
    float pad = std::max(6.f, h * .011f), font = 8.f * std::max(1, h / 720);
    float card_h = (top - 3 * pad - font) / 2, portrait_h = std::max(8.f, card_h - 2 * font - 3);
    Color header = menu_style == ui_preferences::Style::Integrated ? ui_theme::PanelColor
                                                                   : ui_theme::BoxHeader;
    block(geometry, pad, pad, w - 2 * pad, top - 2 * pad, header);
    char heading[32];
    std::snprintf(heading, sizeof(heading), "BOX %02d   %02d", data.active + 1,
                  data.boxes[data.active].count);
    label(geometry, heading, pad * 2, pad * 1.3f, font);
    auto mon = [&](const pc_storage::Mon &m, int index, float x, float y, float cell,
                   float portrait, float glyph_size) {
        const auto &pic = portraits.get(ctx->rom, ctx->rom_size, m.species, true);
        auto rgba = mon_pic::rgba(pic, battle::palette(ctx, m.species));
        glTexSubImage2D(GL_TEXTURE_2D, 0, (index % 8) * 64, (index / 8) * 64, 56, 56, GL_RGBA,
                        GL_UNSIGNED_BYTE, rgba.data());
        ++uploads;
        float mid = x + (cell - portrait) * .5f;
        slots[index] = {mid, y, portrait, portrait};
        panel(geometry, mid, y, portrait, portrait, White,
              {float(index % 8 * 64), float(index / 8 * 64), 56, 56});
        int length = 0;
        while (length < 10 && m.name[length] != 0x50)
            ++length;
        text(geometry, m.name.data(), length, x + (cell - length * glyph_size) * .5f,
             y + portrait + 1, glyph_size);
        char level[8];
        std::snprintf(level, sizeof(level), "L%02d", m.level);
        label(geometry, level, x + (cell - std::strlen(level) * glyph_size) * .5f,
              y + portrait + glyph_size + 2, glyph_size);
    };
    float cell = w / 10.f;
    for (int i = 0; i < data.boxes[data.active].count; i++)
        mon(data.boxes[data.active].mons[i], i, (i % 10) * cell, pad * 2 + font + (i / 10) * card_h,
            cell, portrait_h, font);
    float side = std::max(30.f, left - 3 * pad), row = 144 * scale / 6.f;
    for (int i = 0; i < 12; i++) {
        float x = i < 6 ? pad : w - pad - side, y = top + (i % 6) * row + pad;
        bool current_box = i == data.active;
        block(geometry, x, y, side, row - 2 * pad,
              current_box ? ui_theme::BoxActive : ui_theme::BoxInactive);
        char name[20];
        std::snprintf(name, sizeof(name), "BOX %02d", i + 1);
        float glyph = font;
        label(geometry, name, x + 5, y + 5, glyph);
        std::snprintf(name, sizeof(name), "%02d", data.boxes[i].count);
        label(geometry, name, x + 5, y + glyph + 10, glyph);
        panel(geometry, x + side - 12, y + row - 2 * pad - 10, 6, 3,
              current_box ? ui_theme::BoxActiveLed : ui_theme::BoxInactiveLed);
    }
    float bottom = h - top + pad;
    block(geometry, pad, bottom, w - 2 * pad, top - 2 * pad,
          menu_style == ui_preferences::Style::Integrated ? ui_theme::PanelColor
                                                          : ui_theme::PartyShelf);
    std::snprintf(heading, sizeof(heading), "PARTY %02d", data.party.count);
    label(geometry, heading, pad * 2, bottom + pad, font);
    float party_cell = (w - 4 * pad) / 6.f,
          party_portrait = std::min(56.f, (top - 3 * pad - 3 * font));
    for (int i = 0; i < data.party.count; i++)
        mon(data.party.mons[i], 20 + i, pad * 2 + i * party_cell, bottom + pad + font + 3,
            party_cell, party_portrait, font);
}
bool draw(GBContext *ctx, int w, int h, bool menu_open) {
    presented = false;
    selected = {};
    auto next = pc_storage::read(ctx);
    if (!next.valid)
        return false;
    for (const auto *group : {&next.boxes[next.active], &next.party})
        for (int i = 0; i < group->count; i++)
            if (!portraits.get(ctx->rom, ctx->rom_size, group->mons[i].species, true).valid)
                return false;
    if (!data.valid || data.fingerprint != next.fingerprint || w != width || h != height ||
        rendered_style != menu_style)
        build(ctx, w, h, next);
    selected = pc_box_state::selection(ctx, gb_get_framebuffer(ctx), data);
    firstperson::Camera camera;
    camera.y = 0;
    camera.z = 9 * 1.428148f;
    auto matrix = camera.perspective(float(w) / h);
    glViewport(0, 0, w, h);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glClearDepthf(1);
    glClearColor(ui_theme::BoxBackground.r, ui_theme::BoxBackground.g, ui_theme::BoxBackground.b,
                 1);
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
    auto draw_vertices = [&](const std::vector<Vertex> &mesh_vertices) {
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferData(GL_ARRAY_BUFFER, mesh_vertices.size() * sizeof(Vertex), mesh_vertices.data(),
                     GL_STREAM_DRAW);
        for (int i = 0; i < 3; i++)
            glEnableVertexAttribArray(i);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void *)offsetof(Vertex, p));
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void *)offsetof(Vertex, u));
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void *)offsetof(Vertex, c));
        glDrawArrays(GL_TRIANGLES, 0, GLsizei(mesh_vertices.size()));
    };
    draw_vertices(geometry);
    if (selected.index >= 0) {
        auto r = slots[(selected.party ? 20 : 0) + selected.index];
        std::vector<Vertex> highlight;
        Color amber = ui_theme::BoxSelection;
        float x = r[0] - 2, y = r[1] - 2, s = r[2] + 4;
        panel(highlight, x, y, s, 2, amber, Solid, .04f);
        panel(highlight, x, y + s - 2, s, 2, amber, Solid, .04f);
        panel(highlight, x, y, 2, s, amber, Solid, .04f);
        panel(highlight, x + s - 2, y, 2, s, amber, Solid, .04f);
        draw_vertices(highlight);
    }
    for (int i = 0; i < 3; i++)
        glDisableVertexAttribArray(i);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
    menu_full = true;
    scene_filter::capture(w, h);
    menu_blurred = scene_filter::draw(w, h, .78f, .25f);
    if (!menu_open) {
        if (menu_style == ui_preferences::Style::Integrated)
            full_menu::draw(ctx, pc_state::Mode::Bill, w, h);
        else
            lcd_overlay::framed(gb_get_framebuffer(ctx), float(w), float(h));
    }
    scene_filter::invalidate();
    presented = true;
    return true;
}
} // namespace pc_boxes
