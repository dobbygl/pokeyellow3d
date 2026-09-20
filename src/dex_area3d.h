#pragma once
// Included after the shared geometry primitives. AREA owns its overview mesh,
// atlas and projection; it never changes the resident world's buffers/camera.
namespace dex_area3d {
GLuint texture = 0, mesh = 0, markers_buffer = 0, area_program = 0;
const uint8_t *identity = nullptr;
size_t count = 0, builds = 0, uploads = 0;
float min_x = 0, max_x = 0, min_z = 0, max_z = 0;
bool held = false, presented = false, ready = false, blink = false, title_valid = false;
int species = 0;
dex_nests::Result nests;
std::map<int, dex_nests::Entrance> anchors;
struct Marker {
    float x, z;
    bool land, water;
};
std::vector<Marker> markers;
struct Label {
    float x, z;
    std::vector<uint8_t> name;
};
std::vector<Label> labels;
std::array<uint32_t, 160 * 144> lcd{};
void reset() {
    held = presented = ready = blink = title_valid = false;
    species = 0;
    nests = {};
    markers.clear();
}
void shutdown() {
    if (texture)
        glDeleteTextures(1, &texture);
    if (mesh)
        glDeleteBuffers(1, &mesh);
    if (markers_buffer)
        glDeleteBuffers(1, &markers_buffer);
    if (area_program)
        glDeleteProgram(area_program);
    texture = mesh = markers_buffer = area_program = 0;
    identity = nullptr;
    count = builds = uploads = 0;
    anchors.clear();
    labels.clear();
    reset();
}
bool wants(const GBContext *ctx) {
    if (dex_area_state::active(ctx))
        return true;
    if (!held || !ctx || !ctx->wram || !ctx->rom || !ctx->io || !ctx->vram ||
        ctx->rom != identity || battle::read(ctx, battle::IsInBattle))
        return false;
    // Retain the last valid overview while the original menu restores tiles.
    // A stack return by itself does not mean its new LCD has arrived yet.
    if (!battle::live_return(ctx, 0x4003d, 0x4140) && !battle::live_return(ctx, 0x40061, 0x4070))
        return false;
    return !dex_state::data(ctx) &&
           !dex_state::visible(ctx, gb_get_framebuffer(const_cast<GBContext *>(ctx)),
                               dex_state::list(ctx));
}
void synchronize(const GBContext *ctx) {
    if (!wants(ctx))
        reset();
}
bool initialize(const GBContext *ctx) {
    if (identity == ctx->rom && mesh && area_program)
        return true;
    shutdown();
    try {
        kanto::Rom rom(ctx->rom, ctx->rom_size);
        kanto::World overview(rom);
        overview.discover_warps(rom);
        anchors = dex_nests::entrances(overview);
        std::vector<Vertex> all;
        // create_map has one scratch vector. Restore it even if a ROM read throws.
        struct Scratch {
            std::vector<Vertex> saved;
            Scratch() : saved(std::move(scenery)) {}
            ~Scratch() {
                scenery = std::move(saved);
            }
        } scratch;
        bool first = true;
        for (const auto &entry : overview.maps) {
            const auto &map = entry.second;
            if (map.component != 0)
                continue;
            if (first) {
                min_x = max_x = float(map.origin_x);
                min_z = max_z = float(map.origin_z);
                first = false;
            }
            min_x = std::min(min_x, float(map.origin_x));
            max_x = std::max(max_x, float(map.origin_x + map.width));
            min_z = std::min(min_z, float(map.origin_z));
            max_z = std::max(max_z, float(map.origin_z + map.height));
            create_map(ctx, map, map.block_data);
            all.insert(all.end(), scenery.begin(), scenery.end());
            if (map.id <= 10) {
                Label label{map.origin_x + map.width * .5f, map.origin_z + map.height * .5f, {}};
                size_t at = rom.address(0x1c, rom.word(0x7139d + map.id * 3));
                for (int n = 0; n < 24; n++) {
                    uint8_t c = rom.byte(at + n);
                    if (c == 0x50)
                        break;
                    label.name.push_back(c);
                }
                if (!label.name.empty())
                    labels.push_back(std::move(label));
            }
        }
        if (all.empty())
            return false;
        std::vector<uint8_t> rgba(AW * AH * 4);
        auto put = [&](int x, int y, Color c) {
            size_t at = (y * AW + x) * 4;
            rgba[at] = uint8_t(c.r * 255);
            rgba[at + 1] = uint8_t(c.g * 255);
            rgba[at + 2] = uint8_t(c.b * 255);
            rgba[at + 3] = uint8_t(c.a * 255);
        };
        for (int id : {0, 3, 14, 23}) {
            auto tiles = kanto::read_tileset(rom, id);
            int slot = id == 3 ? 1 : id == 14 ? 2 : id == 23 ? 3 : 0;
            for (int t = 0; t < 96; t++)
                for (int y = 0; y < 8; y++)
                    for (int x = 0; x < 8; x++) {
                        size_t at = tiles.graphics + t * 16 + y * 2;
                        int c = ((rom.byte(at) >> (7 - x)) & 1) |
                                (((rom.byte(at + 1) >> (7 - x)) & 1) << 1);
                        for (int p = 0; p < 3; p++)
                            put(slot * 128 + (t % 16) * 8 + x, p * 128 + (t / 16) * 8 + y,
                                (p == 0   ? ground
                                 : p == 1 ? facade
                                          : water)[c]);
                    }
        }
        for (int g = 0; g < 128; g++)
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++)
                    put((g % 16) * 8 + x, 384 + (g / 16) * 8 + y,
                        {1, 1, 1, float((rom.byte(0x10600 + g * 8 + y) >> (7 - x)) & 1)});
        put(511, 511, White);
        const char *vs = R"(
            attribute vec3 position;attribute vec2 texcoord;attribute vec4 color;
            uniform mat4 projection;uniform vec2 center;
            varying vec2 uv;varying vec4 tint;varying float mist;
            void main(){gl_Position=projection*vec4(position,1.0);uv=texcoord;tint=color;
                if(color.a>1.5){uv=vec2(511.25/512.0);tint.a=1.0;}
                mist=min(0.12,length(position.xz-center)/1800.0);})";
        const char *fs = R"(
            precision mediump float;uniform sampler2D image;
            varying vec2 uv;varying vec4 tint;varying float mist;
            void main(){vec4 c=texture2D(image,uv)*tint;if(c.a<0.08)discard;
                gl_FragColor=vec4(mix(c.rgb,vec3(0.68,0.79,0.75),mist),c.a);})";
        GLuint a = shader(GL_VERTEX_SHADER, vs), b = shader(GL_FRAGMENT_SHADER, fs);
        if (!a || !b) {
            if (a)
                glDeleteShader(a);
            if (b)
                glDeleteShader(b);
            return false;
        }
        area_program = glCreateProgram();
        glAttachShader(area_program, a);
        glAttachShader(area_program, b);
        glBindAttribLocation(area_program, 0, "position");
        glBindAttribLocation(area_program, 1, "texcoord");
        glBindAttribLocation(area_program, 2, "color");
        glLinkProgram(area_program);
        glDeleteShader(a);
        glDeleteShader(b);
        GLint ok = 0;
        glGetProgramiv(area_program, GL_LINK_STATUS, &ok);
        if (!ok)
            return false;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, AW, AH, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glGenBuffers(1, &mesh);
        glBindBuffer(GL_ARRAY_BUFFER, mesh);
        glBufferData(GL_ARRAY_BUFFER, all.size() * sizeof(Vertex), all.data(), GL_STATIC_DRAW);
        glGenBuffers(1, &markers_buffer);
        count = all.size();
        identity = ctx->rom;
        ++builds;
        ++uploads;
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "[AREA] Overview unavailable: %s\n", e.what());
        shutdown();
        return false;
    }
}
void bind(GLuint object) {
    glBindBuffer(GL_ARRAY_BUFFER, object);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, p));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, u));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, c));
}
bool draw(GBContext *ctx, int w, int h, bool menu_open) {
    if (!initialize(ctx))
        return false;
    if (dex_area_state::active(ctx)) {
        int current = battle::read(ctx, dex_state::Current);
        if (current != species) {
            auto next = dex_nests::read(ctx->rom, ctx->rom_size, current);
            if (!next.valid)
                return false;
            std::vector<Marker> next_markers;
            for (auto location : next.locations) {
                auto found = anchors.find(location.map);
                if (found == anchors.end())
                    return false;
                auto a = found->second;
                auto m = std::find_if(next_markers.begin(), next_markers.end(),
                                      [&](const Marker &v) { return v.x == a.x && v.z == a.z; });
                if (m == next_markers.end())
                    next_markers.push_back({a.x, a.z, location.land, location.water});
                else {
                    m->land |= location.land;
                    m->water |= location.water;
                }
            }
            species = current;
            nests = std::move(next);
            markers = std::move(next_markers);
            title_valid = false;
        }
        held = true;
    }
    ready = dex_area_state::ready(ctx);
    blink = dex_area_state::blink(ctx);
    if (dex_area_state::title_ready(ctx, gb_get_framebuffer(ctx))) {
        std::copy_n(gb_get_framebuffer(ctx), lcd.size(), lcd.begin());
        title_valid = true;
    }
    float cx = (min_x + max_x) * .5f, cz = (min_z + max_z) * .5f;
    float scale = std::min((w - 40.f) / (max_x - min_x + 12), (h - 128.f) / (max_z - min_z + 12));
    scale = std::max(.1f, scale);
    firstperson::Matrix matrix{};
    matrix[0] = 2 * scale / w;
    matrix[9] = -2 * scale / h;
    matrix[6] = -1.f / 128;
    matrix[12] = -cx * matrix[0];
    matrix[13] = 2 * scale * cz / h + 80.f / h;
    matrix[15] = 1;
    glViewport(0, 0, w, h);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(.075f, .15f, .17f, 1);
    glDepthMask(GL_TRUE);
    glClearDepthf(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(area_program);
    glUniformMatrix4fv(glGetUniformLocation(area_program, "projection"), 1, GL_FALSE,
                       matrix.data());
    glUniform2f(glGetUniformLocation(area_program, "center"), cx, cz);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(area_program, "image"), 0);
    for (int i = 0; i < 3; i++)
        glEnableVertexAttribArray(i);
    bind(mesh);
    glDrawArrays(GL_TRIANGLES, 0, GLsizei(count));
    if (blink) {
        std::vector<Vertex> v;
        float r = std::max(2.3f, 5.f / scale);
        for (const auto &m : markers) {
            Color c = m.land ? Color{1, .79f, .16f} : Color{.22f, .82f, 1};
            quad(v, {m.x, 14, m.z - r}, {m.x + r, 14, m.z}, {m.x, 14, m.z + r}, {m.x - r, 14, m.z},
                 c);
            quad(v, {m.x, 15, m.z - r * .6f}, {m.x + r * .6f, 15, m.z}, {m.x, 15, m.z + r * .6f},
                 {m.x - r * .6f, 15, m.z}, White);
        }
        bind(markers_buffer);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Vertex), v.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, GLsizei(v.size()));
    }
    for (int i = 0; i < 3; i++)
        glDisableVertexAttribArray(i);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
    if (!menu_open) {
        auto *dl = ImGui::GetForegroundDrawList();
        std::vector<ImVec4> placed;
        for (const auto &label : labels) {
            float x = (label.x - cx) * scale + w * .5f, y = (label.z - cz) * scale + h * .5f - 40;
            float width = float(label.name.size() * 8), left = 0, top = 0;
            for (int attempt = 0; attempt < 12; attempt++) {
                left = std::round(std::clamp(x - width * .5f, 4.f, std::max(4.f, w - width - 4)));
                top = std::round(std::clamp(y - 17 + attempt * 12, 4.f, std::max(4.f, h - 112.f)));
                bool overlap = false;
                for (auto p : placed)
                    overlap |= left < p.z + 4 && left + width > p.x - 4 && top < p.w + 3 &&
                               top + 8 > p.y - 3;
                if (!overlap)
                    break;
            }
            placed.push_back({left, top, left + width, top + 8});
            dl->AddRectFilled({left - 3, top - 3}, {left + width + 3, top + 11},
                              IM_COL32(12, 29, 31, 225));
            for (size_t i = 0; i < label.name.size(); i++) {
                int g = int(label.name[i]) - 0x80;
                if (g < 0 || g >= 128)
                    continue;
                float u = float((g % 16) * 8) / AW, v = float(384 + (g / 16) * 8) / AH;
                dl->AddImage((ImTextureID)(intptr_t)texture, {left + i * 8, top},
                             {left + i * 8 + 8, top + 8}, {u, v}, {u + 8.f / AW, v + 8.f / AH});
            }
        }
        if (title_valid) {
            int s = std::max(1, std::min(w / 160, 3));
            float x = std::floor((w - 160 * s) * .5f), y = float(h - 8 * s - 16);
            auto region = [&](lcd_overlay::Rect r, float left, float top) {
                if (lcd_overlay::upload(lcd.data(), r))
                    dl->AddImage((ImTextureID)(intptr_t)lcd_overlay::texture, {left, top},
                                 {left + r.w * 8 * s, top + r.h * 8 * s}, {r.x / 20.f, r.y / 18.f},
                                 {(r.x + r.w) / 20.f, (r.y + r.h) / 18.f});
            };
            dl->AddRectFilled({x - 4, y - 4}, {x + 160 * s + 4, y + 8 * s + 4},
                              IM_COL32(12, 29, 31, 255));
            region({0, 0, 20, 1}, x, y);
            if (nests.locations.empty())
                region({1, 7, 17, 4}, std::floor((w - 136 * s) * .5f),
                       std::floor((h - 32 * s) * .5f));
        }
    }
    presented = true;
    return true;
}
} // namespace dex_area3d
