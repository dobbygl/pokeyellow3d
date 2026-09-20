#pragma once
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
void shutdown() {
    if (texture)
        glDeleteTextures(1, &texture);
    texture = 0;
    species = 0;
    picture = {};
    image = {};
    lcd_valid = presented = false;
    uploads = 0;
    cache = {};
    selection = {};
    list_mode = false;
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
void lcd_region(std::vector<Vertex> &v, lcd_overlay::Rect r, float left, float top, float scale) {
    panel(v, left, top, r.w * 8 * scale, r.h * 8 * scale, White,
          {float(r.x * 8), float(64 + r.y * 8), float(r.w * 8), float(r.h * 8)});
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
            rgba[i * 4] = lcd[i] >> 16;
            rgba[i * 4 + 1] = lcd[i] >> 8;
            rgba[i * 4 + 2] = lcd[i];
            rgba[i * 4 + 3] = 255;
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 64, 160, 144, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        ++uploads;
    }
    std::vector<Vertex> v;
    box(v, -6.8f, -.35f, 6.65f, .7f, -3.5f, 4, {.66f, .065f, .09f});
    box(v, .15f, -.35f, 6.65f, .7f, -3.5f, 4, {.76f, .085f, .11f});
    box(v, -.15f, -.22f, .3f, .65f, -3.2f, 3.7f, {.28f, .055f, .065f});
    panel(v, -6.32f, 3.48f, 5.7f, 6.45f, {.085f, .12f, .12f}, Solid, .38f);
    panel(v, .63f, 2.8f, 5.6f, 5.65f,
          list_mode ? Color{.50f, .61f, .53f} : Color{.095f, .17f, .15f}, Solid, .38f);
    panel(v, -6.12f, 3.25f, 5.3f, 5.95f, White, Solid, .40f);
    // Original number/name/category/height/weight and description pixels. No
    // typeface or menu content is reconstructed; the source portrait is moved
    // to the other screen rather than duplicated with the text.
    if (list_mode) {
        lcd_region(v, {0, 0, 14, 18}, -5.68f, 3.15f, .039f);
        panel(v, .82f, 2.02f, 3.62f, 3.62f, White, {192, 0, 56, 56});
        lcd_region(v, {15, 8, 5, 9}, 4.48f, 1.68f, .039f);
        // The counters on the body are also the game's own glyphs, driven by
        // the same seen/owned bitmaps exposed in Selection, never re-typeset.
        lcd_region(v, {16, 1, 4, 2}, 2.0f, 3.57f, .049f);
        lcd_region(v, {16, 4, 4, 2}, 4.05f, 3.57f, .049f);
    } else {
        lcd_region(v, {2, 8, 5, 1}, -5.85f, 3.1f, .041f);
        lcd_region(v, {9, 2, 10, 7}, -5.5f, 2.55f, .052f);
        lcd_region(v, {1, 10, 18, 7}, -6.1f, -.52f, .0365f);
        panel(v, .82f, 2.65f, 5.2f, 5.2f, White, {192, 0, 56, 56});
    }
    box(v, .7f, .35f, .55f, .18f, 3.18f, 3.73f, {.15f, .69f, .9f});
    if (!list_mode) {
        box(v, 1.6f, .35f, .22f, .13f, 3.32f, 3.54f, {.5f, .83f, .24f});
        box(v, 2.1f, .35f, .22f, .13f, 3.32f, 3.54f, {.98f, .77f, .2f});
        for (int i = 0; i < 4; i++)
            panel(v, 4.15f + i * .32f, 3.53f, .14f, .48f, {.23f, .035f, .045f});
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
    glClearColor(.035f, .065f, .075f, 1);
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
    presented = true;
    return true;
}
} // namespace dex3d
