#pragma once
// Independent gallery resources. All team data is read from bank-zero SRAM;
// the retained interior and its camera are never changed by this scene.
namespace pc_hall3d {
GLuint texture = 0;
mon_pic::Cache portraits;
pc_details::HallTeam team;
std::vector<Vertex> geometry;
bool presented = false, initialized = false;
float camera_x = 0;
uint32_t last_cycles = 0;
size_t builds = 0, uploads = 0;
void reset() {
    team = {};
    geometry.clear();
    presented = initialized = false;
    camera_x = 0;
    last_cycles = 0;
}
void shutdown() {
    if (texture)
        glDeleteTextures(1, &texture);
    texture = 0;
    reset();
    portraits = {};
    builds = uploads = 0;
}
void initialize(const GBContext *ctx) {
    if (texture)
        return;
    std::vector<uint8_t> rgba(AW * AH * 4);
    rom_font::copy_to(ctx->rom, ctx->rom_size, rgba.data(), AW, AH, 0, 256);
    for (int c = 0; c < 4; ++c)
        rgba[((AH - 1) * AW + AW - 1) * 4 + c] = 255;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, AW, AH, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
}
void text(std::vector<Vertex> &v, const uint8_t *chars, int length, float mid, float y) {
    int count = 0;
    while (count < length && chars[count] != 0x50)
        ++count;
    constexpr float size = .12f;
    for (int i = 0; i < count; ++i) {
        int glyph = chars[i] - 0x80;
        if (glyph < 0 || glyph >= 128)
            continue;
        float x = mid + (i - count * .5f) * size;
        quad(v, {x, y, .72f}, {x + size, y, .72f}, {x + size, y - size, .72f}, {x, y - size, .72f},
             {.94f, .93f, .81f},
             {float(glyph % 16 * 8) - .25f, float(256 + glyph / 16 * 8) - .25f, 8.5f, 8.5f});
    }
}
void build(const GBContext *ctx, const pc_details::HallTeam &next) {
    initialize(ctx);
    geometry.clear();
    ++builds;
    float end = (next.count - 1) * 2.7f;
    box(geometry, -4, -3, end + 8, 5, -.4f, 0, {.12f, .19f, .21f});
    box(geometry, -4, -2.8f, end + 8, .2f, 0, 5, {.09f, .14f, .17f});
    glBindTexture(GL_TEXTURE_2D, texture);
    for (int i = 0; i < next.count; ++i) {
        const auto &mon = next.mons[i];
        float x = i * 2.7f;
        box(geometry, x - 1.05f, -.8f, 2.1f, 1.6f, 0, .16f, {.30f, .33f, .31f});
        box(geometry, x - .85f, -.6f, 1.7f, 1.2f, .16f, 1.05f, {.22f, .29f, .29f});
        box(geometry, x - .95f, -.7f, 1.9f, 1.4f, 1.05f, 1.2f, {.57f, .50f, .30f});
        auto rgba = mon_pic::rgba(portraits.get(ctx->rom, ctx->rom_size, mon.species),
                                  battle::palette(ctx, mon.species));
        glTexSubImage2D(GL_TEXTURE_2D, 0, i * 64, 0, 56, 56, GL_RGBA, GL_UNSIGNED_BYTE,
                        rgba.data());
        ++uploads;
        quad(geometry, {x - 1, 3.35f, 0}, {x + 1, 3.35f, 0}, {x + 1, 1.35f, 0}, {x - 1, 1.35f, 0},
             White, {float(i * 64) - .25f, -.25f, 56.5f, 56.5f});
        text(geometry, mon.name.data(), int(mon.name.size()), x, .83f);
        uint8_t level[] = {0x8b, uint8_t(0xf6 + mon.level / 100),
                           uint8_t(0xf6 + mon.level / 10 % 10), uint8_t(0xf6 + mon.level % 10)};
        text(geometry, level, 4, x, .59f);
    }
}
bool draw(GBContext *ctx, int w, int h, bool menu_open) {
    presented = false;
    auto next = pc_details::hall(ctx);
    if (!next.valid || !ctx->vram)
        return false;
    const auto &current = portraits.get(ctx->rom, ctx->rom_size, next.mons[next.selected].species);
    if (!current.valid ||
        std::memcmp(current.tiles.data(), ctx->vram + 0x1000, mon_pic::TileBytes) ||
        !battle::rectangle(ctx, {12, 5, 0}, true))
        return false;
    for (int i = 0; i < next.count; ++i)
        if (!portraits.get(ctx->rom, ctx->rom_size, next.mons[i].species).valid)
            return false;
    if (!team.valid || team.fingerprint != next.fingerprint)
        build(ctx, next);
    uint32_t now = uint32_t(ctx->cycles);
    float target = next.selected * 2.7f;
    if (!initialized || team.index != next.index)
        camera_x = target;
    else if (!menu_open && window_focused) {
        float dt = std::min(.025f, float(now - last_cycles) / 4194304.f);
        camera_x += (target - camera_x) * (1 - std::exp(-dt * 9));
    }
    initialized = true;
    last_cycles = now;
    team = next;
    int scale = std::max(1, std::min(w / 264, h / 300));
    int footer = 88 * scale + 24;
    int gallery_height = std::max(1, h - footer);
    firstperson::Camera camera;
    camera.x = camera_x;
    camera.y = 1.8f;
    camera.z = 6.8f;
    auto matrix = camera.perspective(float(w) / gallery_height);
    glViewport(0, footer, w, gallery_height);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glClearDepthf(1);
    glClearColor(.035f, .055f, .065f, 1);
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
    glBufferData(GL_ARRAY_BUFFER, geometry.size() * sizeof(Vertex), geometry.data(),
                 GL_STREAM_DRAW);
    for (int i = 0; i < 3; ++i)
        glEnableVertexAttribArray(i);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, p));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, u));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, c));
    glDrawArrays(GL_TRIANGLES, 0, GLsizei(geometry.size()));
    for (int i = 0; i < 3; ++i)
        glDisableVertexAttribArray(i);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, w, h);
    menu_full = true;
    menu_blurred = false;
    if (!menu_open && lcd_overlay::upload(gb_get_framebuffer(ctx), lcd_overlay::Full)) {
        float left = std::floor((w - 264 * scale) * .5f), top = float(h - footer + 12);
        auto region = [&](lcd_overlay::Rect r, float x, float y) {
            ImGui::GetForegroundDrawList()->AddImage(
                (ImTextureID)(intptr_t)lcd_overlay::texture, {x, y},
                {x + r.w * 8 * scale, y + r.h * 8 * scale}, {r.x / 20.f, r.y / 18.f},
                {(r.x + r.w) / 20.f, (r.y + r.h) / 18.f});
        };
        region({0, 2, 12, 11}, left, top);
        region({0, 13, 20, 4}, left + 104 * scale, top + 28 * scale);
    }
    presented = true;
    return true;
}
} // namespace pc_hall3d
