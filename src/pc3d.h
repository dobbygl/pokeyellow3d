#pragma once
// Included after the resident-world renderer. Only the copied projection is
// animated; no cached world matrix, mesh, actor or game memory is changed.
namespace pc3d {
GLuint texture = 0;
std::array<uint32_t, 160 * 144> lcd{};
bool lcd_valid = false, presented = false, monitor_image = false;
firstperson::Matrix matrix{};
std::array<float, 8> screen{};
pc_details::Items stored_items;
pc_details::Rating dex_rating;
void reset() {
    pc_motion = {};
    pc_sample = {};
    pc_terminal = {};
    pc_composed = false;
    presented = monitor_image = lcd_valid = false;
    pc_boxes::reset();
    stored_items = {};
    dex_rating = {};
}
void shutdown() {
    if (texture)
        glDeleteTextures(1, &texture);
    texture = 0;
    reset();
    pc_boxes::shutdown();
}
void upload(const GBContext *ctx, const uint32_t *original) {
    if (!texture) {
        std::vector<uint8_t> pixels(AW * AH * 4);
        for (int glyph = 0; glyph < 128; glyph++)
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) {
                    size_t dest = ((256 + glyph / 16 * 8 + y) * AW + glyph % 16 * 8 + x) * 4;
                    bool ink = ctx->rom[0x10600 + glyph * 8 + y] & (1 << (7 - x));
                    for (int c = 0; c < 3; c++)
                        pixels[dest + c] = 255;
                    pixels[dest + 3] = ink ? 255 : 0;
                }
        for (int i = 0; i < 4; i++)
            pixels[((AH - 1) * AW + AW - 1) * 4 + i] = 255;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, AW, AH, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     pixels.data());
    }
    if (!original || (lcd_valid && !std::memcmp(lcd.data(), original, sizeof(lcd))))
        return;
    std::copy_n(original, lcd.size(), lcd.begin());
    lcd_valid = true;
    std::array<uint8_t, 160 * 144 * 4> pixels{};
    for (size_t i = 0; i < lcd.size(); i++) {
        pixels[i * 4] = lcd[i] >> 16;
        pixels[i * 4 + 1] = lcd[i] >> 8;
        pixels[i * 4 + 2] = lcd[i];
        pixels[i * 4 + 3] = 255;
    }
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 160, 144, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
}
void draw(GBContext *ctx, int w, int h, bool menu_open) {
    stored_items =
        pc_sample.mode == pc_state::Mode::Items ? pc_details::items(ctx) : pc_details::Items{};
    dex_rating =
        pc_sample.mode == pc_state::Mode::Oak ? pc_details::rating(ctx) : pc_details::Rating{};
    bool counters = stored_items.valid || dex_rating.valid;
    if (pc_sample.mode == pc_state::Mode::Bill && pc_boxes::draw(ctx, w, h, menu_open)) {
        monitor_image = false;
        presented = true;
        return;
    }
    auto pc = pc_terminal;
    float t = pc_motion.eased();
    bool open = pc_sample.mode != pc_state::Mode::None;
    Vec center{pc.x + .5f, pc.height + .47f, pc.z + .195f};
    firstperson::Camera close;
    close.x = center.x;
    close.y = center.y;
    close.z = center.z + std::max(.87f, .48f * 1.428148f / (float(w) / h * .76f));
    if (pc_sample.mode == pc_state::Mode::Items || pc_sample.mode == pc_state::Mode::Oak) {
        close.z = center.z + (close.z - center.z) * 1.16f;
        close.y -= .04f;
    }
    close.yaw = 0;
    auto target = close.perspective(float(w) / h);
    matrix = world_frame.matrix;
    auto normalize = [&](firstperson::Matrix &m) {
        float divisor = m[3] * center.x + m[7] * center.y + m[11] * center.z + m[15];
        if (divisor > .0001f)
            for (auto &value : m)
                value /= divisor;
    };
    normalize(matrix);
    normalize(target);
    for (size_t i = 0; i < 16; i++)
        matrix[i] = matrix[i] * (1 - t) + target[i] * t;
    draw_world_frame(w, h, {}, t > 0, false, 0, &matrix);
    bool full = menu_regions.kind == menu_layout::Kind::Full;
    bool main = open && ((pc_sample.main && full) || pc_sample.mode == pc_state::Mode::Oak);
    if (main) {
        upload(ctx, gb_get_framebuffer(ctx));
        monitor_image = true;
    } else {
        upload(ctx, nullptr);
        if (open)
            monitor_image = false;
    }
    std::vector<Vertex> v;
    box(v, pc.x - .02f, pc.z + .04f, 1.04f, .15f, pc.height, pc.height + .94f,
        {.28f, .34f, .32f, t});
    box(v, pc.x + .34f, pc.z + .07f, .32f, .26f, pc.height - .03f, pc.height + .025f,
        {.22f, .27f, .25f, t});
    Vec corners[] = {{center.x - .48f, center.y + .432f, center.z},
                     {center.x + .48f, center.y + .432f, center.z},
                     {center.x + .48f, center.y - .432f, center.z},
                     {center.x - .48f, center.y - .432f, center.z}};
    quad(v, corners[0], corners[1], corners[2], corners[3],
         monitor_image ? Color{1, 1, 1, t} : Color{.17f, .32f, .26f, t},
         // Cancel the atlas helper's quarter-texel inset: every LCD pixel has
         // equal width here, including the first and last rows/columns.
         monitor_image ? UV{-.25f, -.25f, 160.5f, 144.5f} : Solid);
    if (counters) {
        box(v, pc.x - .02f, pc.z + .04f, 1.04f, .15f, pc.height - .13f, pc.height + .04f,
            {.18f, .25f, .23f, t});
        char text[32];
        if (stored_items.valid)
            std::snprintf(text, sizeof(text), "ITEMS %02d OF 50", stored_items.count);
        else
            std::snprintf(text, sizeof(text), "SEEN %03d CAUGHT %03d", dex_rating.seen,
                          dex_rating.caught);
        float glyph_size = .035f, left = center.x - std::strlen(text) * glyph_size * .5f;
        for (size_t i = 0; text[i]; ++i) {
            int glyph = text[i] >= '0' && text[i] <= '9'   ? 0x76 + text[i] - '0'
                        : text[i] >= 'A' && text[i] <= 'Z' ? text[i] - 'A'
                                                           : -1;
            if (glyph < 0)
                continue;
            float x = left + i * glyph_size, y = pc.height - .04f, z = center.z + .001f;
            quad(v, {x, y, z}, {x + glyph_size, y, z}, {x + glyph_size, y - glyph_size, z},
                 {x, y - glyph_size, z}, {.94f, .93f, .81f, t},
                 {float(glyph % 16 * 8) - .25f, float(256 + glyph / 16 * 8) - .25f, 8.5f, 8.5f});
        }
    }
    for (int i = 0; i < 4; i++) {
        auto p = corners[i];
        float d = matrix[3] * p.x + matrix[7] * p.y + matrix[11] * p.z + matrix[15];
        screen[i * 2] =
            ((matrix[0] * p.x + matrix[4] * p.y + matrix[8] * p.z + matrix[12]) / d + 1) * w / 2;
        screen[i * 2 + 1] =
            (1 - (matrix[1] * p.x + matrix[5] * p.y + matrix[9] * p.z + matrix[13]) / d) * h / 2;
    }
    glUseProgram(program);
    glUniformMatrix4fv(matrix_loc, 1, GL_FALSE, matrix.data());
    glUniform2f(fade_loc, 1, 0);
    glUniform1f(fog_loc, 0);
    glUniform1f(sky_loc, 0);
    glUniform1f(xray_loc, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(program, "image"), 0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
    if (open && !main) {
        if (full) {
            scene_filter::capture(w, h);
            menu_blurred = scene_filter::draw(w, h);
            if (!menu_open)
                lcd_overlay::framed(gb_get_framebuffer(ctx), float(w), float(h));
        } else if (!menu_open)
            lcd_overlay::regions(gb_get_framebuffer(ctx), menu_regions, float(w), float(h));
    }
    scene_filter::invalidate();
    presented = true;
}
} // namespace pc3d
