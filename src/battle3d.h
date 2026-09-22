#pragma once
// Included by pallet3d.cpp after its GL primitives. This scene owns its texture,
// camera and presentation state; it never consumes or changes map meshes.
namespace battle3d {
GLuint texture = 0;
struct Portrait {
    battle::Image image{};
    int species = 0;
    float alpha = 0, hp = 0, damage = 0;
    int last_hp = 0;
    uint64_t fingerprint = 0;
};
std::array<Portrait, 2> portraits;
uint64_t last_cycles = 0;
uint64_t effect_started = 0;
bool shown = false, full_overlay = false;
float overlay_alpha = 0, effect_time = 0;
int effect_id = 0, effect_actor = 0;
battle::Effect effect_kind = battle::Effect::Original;
bool capture_effect = false;
bool framed_overlay = false;
bool integrated_menu = false;
battle_menu::Kind menu_kind = battle_menu::Kind::Unknown;
int menu_panels = 0;
int trainer_class = 0;
int terrain = 0; // dirt, grass, water, cave, gym
bool terrain_known = false;
int terrain_map = -1, terrain_x = -1, terrain_z = -1;

void remember_terrain(const GBContext *ctx) {
    const auto *s = pallet::scene(pallet::read(ctx, pallet::Map));
    if (!s)
        return;
    int x = pallet::read(ctx, pallet::X), z = pallet::read(ctx, pallet::Y);
    terrain_map = s->id;
    terrain_x = x;
    terrain_z = z;
    int tile = pallet::map_tile(ctx->rom, *s, x * 2, z * 2 + 1);
    if (!pallet::read(ctx, pallet::Battle) && pallet::valid_live_map(ctx, *s)) {
        const auto &ts = pallet::tileset(*s);
        int gx = x * 2, gz = z * 2 + 1;
        int block =
            pallet::read(ctx, uint16_t(0xc6e8 + (gz / 4 + 3) * (s->width / 2 + 6) + gx / 4 + 3));
        tile = ctx->rom[ts.blocks + block * 16 + (gz % 4) * 4 + gx % 4];
    }
    terrain = interior::cave(*s)                               ? 3
              : s->tileset == 7                                ? 4
              : pallet::read(ctx, 0xd6ff) == 2 || tile == 0x14 ? 2
              : tile == pallet::tileset(*s).grass              ? 1
                                                               : 0;
    terrain_known = true;
}
void reset() {
    portraits = {};
    integrated_menu = false;
    menu_kind = battle_menu::Kind::Unknown;
    menu_panels = 0;
    last_cycles = 0;
    shown = false;
    full_overlay = framed_overlay = false;
    overlay_alpha = effect_time = 0;
    effect_id = effect_actor = 0;
    effect_started = 0;
    effect_kind = battle::Effect::Original;
    capture_effect = false;
    trainer_class = 0;
}
void shutdown() {
    if (texture)
        glDeleteTextures(1, &texture);
    texture = 0;
    terrain_known = false;
    reset();
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
firstperson::Matrix camera(float aspect, double time) {
    float ex = float(.10f * std::sin(time * .25)), ey = float(5.4f + .06f * std::sin(time * .7)),
          ez = 10;
    float s = .40f, c = std::sqrt(1 - s * s), f = 1 / std::tan(48.f * firstperson::Pi / 360);
    constexpr float near = .1f, far = 80;
    float a = (far + near) / (near - far), b = 2 * near * far / (near - far);
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
void platform(std::vector<Vertex> &v, float x, float z, Color c) {
    constexpr float radius = 1.8f, y = .12f;
    for (int i = 0; i < 40; i++) {
        float a = i * 2 * firstperson::Pi / 40, b = (i + 1) * 2 * firstperson::Pi / 40;
        Vec p{x + std::cos(a) * radius, y, z + std::sin(a) * radius};
        Vec q{x + std::cos(b) * radius, y, z + std::sin(b) * radius};
        quad(v, {x, y, z}, p, q, q, c);
        quad(v, p, {p.x, 0, p.z}, {q.x, 0, q.z}, q, shade(c, .6f));
    }
}
void orb(std::vector<Vertex> &v, Vec center, float radius, Color color, bool ball = false) {
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 16; col++) {
            auto point = [&](int r, int c) {
                float a = firstperson::Pi * (float(r) / 8 - .5f), b = 2 * firstperson::Pi * c / 16;
                return Vec{center.x + radius * std::cos(a) * std::cos(b),
                           center.y + radius * std::sin(a),
                           center.z + radius * std::cos(a) * std::sin(b)};
            };
            Color shade_color = ball ? (row < 4 ? ui_theme::BallTop : ui_theme::BallBottom) : color;
            if (ball && (row == 3 || row == 4))
                shade_color = ui_theme::BallSeam;
            quad(v, point(row, col), point(row, col + 1), point(row + 1, col + 1),
                 point(row + 1, col), shade(shade_color, .7f + .3f * row / 7));
        }
}
Color move_color(int type) {
    switch (type) {
    case 20:
        return ui_theme::Fire;
    case 21:
        return ui_theme::Water;
    case 22:
        return ui_theme::Grass;
    case 23:
        return ui_theme::Electric;
    case 24:
        return ui_theme::Psychic;
    case 25:
        return ui_theme::Ice;
    default:
        return ui_theme::MoveNeutral;
    }
}
void effects(std::vector<Vertex> &v, const GBContext *ctx) {
    if (capture_effect) {
        int id = battle::read(ctx, battle::Animation),
            counter = battle::read(ctx, battle::AnimCounter);
        float t = std::clamp((11.f - counter) / 10, 0.f, 1.f);
        bool toss = id == 0xc1 || id == 0xc5 || id == 0xc6;
        float wobble = id == 0xc2 ? std::sin((4 - counter) * firstperson::Pi * .5f) * .22f : 0;
        Vec p = toss ? Vec{-2 + 4 * t, 1.1f + 3 * std::sin(t * firstperson::Pi), 2 - 5 * t}
                     : Vec{2 + wobble, .46f, -3};
        orb(v, p, .3f, White, true);
        if (id == 0xc3)
            for (int i = 0; i < 12; i++) {
                float a = 2 * firstperson::Pi * i / 12, r = .4f + .9f * std::fmod(effect_time, 1.f);
                orb(v, {2 + std::cos(a) * r, 1.4f + std::sin(a) * r, -3}, .10f,
                    ui_theme::CaptureSpark);
            }
        return;
    }
    if (effect_kind == battle::Effect::Original || effect_kind == battle::Effect::Physical)
        return;
    auto move = battle::move(ctx, effect_id);
    Color color = move_color(move.type);
    int target = effect_kind == battle::Effect::Self ? effect_actor : 1 - effect_actor;
    float tx = target ? 2.f : -2.f, tz = target ? -3.f : 2.f;
    float t = std::fmod(effect_time * 1.6f, 1.f);
    if (effect_kind == battle::Effect::Projectile) {
        float sx = effect_actor ? 2.f : -2.f, sz = effect_actor ? -3.f : 2.f;
        for (int i = 6; i >= 0; i--) {
            float p = std::clamp(t - i * .035f, 0.f, 1.f);
            orb(v,
                {sx + (tx - sx) * p, 1.6f + .5f * std::sin(p * firstperson::Pi),
                 sz + (tz - sz) * p},
                .20f - i * .02f, shade(color, 1 - i * .08f));
        }
    } else {
        for (int i = 0; i < 16; i++) {
            float a = i * 2 * firstperson::Pi / 16 + effect_time * 3,
                  r = .8f + .3f * std::sin(effect_time * 9);
            float height = effect_kind == battle::Effect::Self
                               ? .3f + 2.8f * std::fmod(t + i / 16.f, 1.f)
                               : 1.6f + std::sin(a) * 1.2f;
            orb(v, {tx + std::cos(a) * r, height, tz + .2f * std::sin(a)}, .085f, color);
        }
    }
}
void panel(const GBContext *ctx, bool enemy, float w, float h) {
    auto m = battle::mon(ctx, enemy);
    auto *dl = ImGui::GetForegroundDrawList();
    float scale = std::max(.8f, std::min(w / 800.f, h / 720.f));
    float pw = 250 * scale, ph = 100 * scale;
    float x = enemy ? 24 * scale : w - pw - 24 * scale, y = enemy ? 22 * scale : h * .43f;
    dl->AddRectFilled({x, y}, {x + pw, y + ph}, ui_theme::Panel, ui_theme::Radius * scale);
    char label[80];
    std::snprintf(label, sizeof label, "%s   Lv.%d", m.name.c_str(), m.level);
    bool styled = menu_style == ui_preferences::Style::Integrated;
    if (styled) {
        int detail = std::max(1, int(scale)),
            name_scale = std::max(1, int(ui_theme::HudGlyphScale * scale));
        hud_text::draw(ctx->rom, ctx->rom_size, ctx->wram + (enemy ? 0xfd9 : 0x1008), 10,
                       {x + ui_theme::Padding * scale, y + ui_theme::Padding * scale}, name_scale,
                       ui_theme::Ink);
        std::snprintf(label, sizeof label, "Lv.%d", m.level);
        hud_text::label(ctx->rom, ctx->rom_size, label,
                        {x + pw - ui_theme::Padding * scale - std::strlen(label) * 8 * detail,
                         y + ui_theme::Padding * scale + 4},
                        detail, ui_theme::DimInk);
    } else
        dl->AddText(nullptr, 18 * scale, {x + ui_theme::Padding * scale, y + 12 * scale},
                    ui_theme::Ink, label);
    int index = enemy ? 1 : 0;
    float hp = std::clamp(portraits[index].hp / std::max(1, m.max_hp), 0.f, 1.f);
    ImU32 color = m.hp_color == 2   ? ui_theme::HpRed
                  : m.hp_color == 1 ? ui_theme::HpAmber
                                    : ui_theme::HpGreen;
    dl->AddRectFilled({x + ui_theme::Padding * scale, y + 42 * scale},
                      {x + pw - ui_theme::Padding * scale, y + 50 * scale}, ui_theme::BarTrack,
                      ui_theme::BarRadius * scale);
    dl->AddRectFilled({x + ui_theme::Padding * scale, y + 42 * scale},
                      {x + ui_theme::Padding * scale + (pw - 28 * scale) * hp, y + 50 * scale},
                      color, ui_theme::BarRadius * scale);
    std::snprintf(label, sizeof label, "%d / %d   %s", m.hp, m.max_hp, battle::status(m.status));
    if (styled)
        hud_text::label(ctx->rom, ctx->rom_size, label,
                        {x + ui_theme::Padding * scale, y + 58 * scale}, std::max(1, int(scale)),
                        ui_theme::DimInk);
    else
        dl->AddText(nullptr, 13 * scale, {x + ui_theme::Padding * scale, y + 58 * scale},
                    ui_theme::DimInk, label);
    if (!enemy) {
        float xp = battle::experience(ctx);
        dl->AddRectFilled({x + ui_theme::Padding * scale, y + 83 * scale},
                          {x + pw - ui_theme::Padding * scale, y + 87 * scale}, ui_theme::BarTrack,
                          styled ? ui_theme::BarRadius * scale : 0);
        dl->AddRectFilled({x + ui_theme::Padding * scale, y + 83 * scale},
                          {x + ui_theme::Padding * scale + (pw - 28 * scale) * xp, y + 87 * scale},
                          ui_theme::Experience, styled ? ui_theme::BarRadius * scale : 0);
    }
    if (battle::read(ctx, battle::IsInBattle) == 2) {
        int count = battle::read(ctx, enemy ? 0xd89b : 0xd162);
        for (int i = 0; i < std::min(count, 6); i++) {
            int party_hp = battle::word(ctx, (enemy ? 0xd8a4 : 0xd16b) + 44 * i);
            dl->AddCircleFilled({x + 12 * scale + i * 13 * scale, y + ph + 12 * scale}, 4 * scale,
                                party_hp ? ui_theme::PartyAlive : ui_theme::PartyFainted);
        }
    }
}
void draw(GBContext *ctx, int w, int h, bool menu_open, bool opening = false) {
    menu_text::prime();
    integrated_menu = false;
    menu_kind = battle_menu::Kind::Unknown;
    menu_panels = 0;
    bool styled = menu_style == ui_preferences::Style::Integrated;
    auto menu = styled ? battle_menu::classify(ctx->wram + 0x3a0) : battle_menu::Layout{};
    if (!terrain_known || terrain_map != pallet::read(ctx, pallet::Map) ||
        terrain_x != pallet::read(ctx, pallet::X) || terrain_z != pallet::read(ctx, pallet::Y) ||
        (last_cycles && ctx->cycles < last_cycles))
        remember_terrain(ctx);
    initialize_texture();
    glBindTexture(GL_TEXTURE_2D, texture);
    float dt = last_cycles && ctx->cycles > last_cycles
                   ? std::min(.1f, float(ctx->cycles - last_cycles) / 4194304)
                   : 1.f / 60;
    if (!shown || ctx->cycles < last_cycles) {
        portraits = {};
        overlay_alpha = effect_time = 0;
        effect_id = effect_actor = 0;
        effect_started = ctx->cycles;
        effect_kind = battle::Effect::Original;
        capture_effect = false;
    }
    last_cycles = ctx->cycles;
    shown = true;
    bool intro = battle::trainer_intro(ctx) ||
                 (opening && battle::normal(ctx) && battle::read(ctx, battle::IsInBattle) == 2 &&
                  battle::read(ctx, 0xcfe7) == 255);
    trainer_class = intro ? battle::read(ctx, 0xd030) : 0;
    bool animation = battle::animation_running(ctx);
    bool cached = portraits[0].fingerprint && portraits[1].fingerprint &&
                  portraits[0].species == battle::read(ctx, 0xd013) &&
                  portraits[1].species == battle::read(ctx, 0xcfe4);
    // The PP/type window covers part of the original back sprite. Keep the
    // already decoded portraits while selecting moves; a cold load without
    // those complete images retains the original framed LCD.
    bool moves = styled && !animation && cached && menu.kind == battle_menu::Kind::Moves &&
                 battle::ready(ctx) && battle::rectangle(ctx, battle::Enemy, true);
    int actor = ctx->hram[0x73] ? 1 : 0;
    // Some boosts (Agility) only switch palettes for two engine frames. Give
    // their confirmed event a bounded cosmetic tail while its result text is
    // printed; never prolong the engine animation or delay the next action.
    bool tail = !animation && cached && effect_kind == battle::Effect::Self &&
                effect_actor == actor && ctx->cycles >= effect_started &&
                ctx->cycles - effect_started < uint64_t(.55 * 4194304);
    int id = animation ? battle::read(ctx, battle::Animation) : tail ? effect_id : 0;
    if (id != effect_id || actor != effect_actor) {
        effect_started = ctx->cycles;
        effect_time = 0;
    } else
        effect_time = float(ctx->cycles - effect_started) / 4194304;
    effect_id = id;
    effect_actor = actor;
    capture_effect = cached && battle::capture_running(ctx);
    effect_kind = tail ? battle::Effect::Self
                  : animation && cached && !(battle::read(ctx, 0xd354) & 0x80)
                      ? battle::move(ctx, id).presentation
                      : battle::Effect::Original;
    bool effect = capture_effect || effect_kind != battle::Effect::Original;
    full_overlay = !opening && !intro && !effect && !moves &&
                   (animation || !battle::rectangle(ctx, battle::Enemy, true) ||
                    !battle::rectangle(ctx, battle::Player, true));
    // D049..D055 holds twelve trainer glyphs plus a terminator. Pokemon
    // nicknames have the shorter ten-glyph limit used by their status panels.
    bool unsupported_name = styled && (intro ? !hud_text::supported(ctx->wram + 0x1049, 13)
                                             : (battle::name_matches(ctx, 0xcfd9, 1, 0) &&
                                                !hud_text::supported(ctx->wram + 0xfd9, 10)) ||
                                                   (battle::name_matches(ctx, 0xd008, 10, 7) &&
                                                    !hud_text::supported(ctx->wram + 0x1008, 10)));
    if (unsupported_name) {
        full_overlay = true;
        overlay_alpha = 1;
    }
    overlay_alpha += std::clamp((full_overlay ? 1.f : 0.f) - overlay_alpha, -dt * 12, dt * 12);
    if (full_overlay)
        framed_overlay = !animation;
    if (overlay_alpha == 0)
        framed_overlay = false;
    for (int i = 0; i < 2; i++) {
        auto m = battle::mon(ctx, i == 1);
        auto &p = portraits[i];
        if (opening && !intro && !i && !battle::name_matches(ctx, 0xd008, 10, 7)) {
            p = {};
            continue;
        }
        if (intro) {
            if (!i) {
                p = {};
                continue;
            }
            if (p.species != -trainer_class) {
                p = {};
                p.species = -trainer_class;
            }
            auto image = battle::portrait(ctx, battle::Enemy, 0);
            if (image != p.image) {
                p.image = image;
                p.fingerprint = battle::fingerprint(image);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 64, 0, 56, 56, GL_RGBA, GL_UNSIGNED_BYTE,
                                image.data());
            }
            p.alpha = std::min(1.f, p.alpha + dt * 7);
            continue;
        }
        if (p.species != m.species) {
            p = {};
            p.species = m.species;
            p.hp = float(m.hp);
            p.last_hp = m.hp;
        } else
            p.hp += (m.hp - p.hp) * (1 - std::exp(-dt * 12));
        if (m.hp < p.last_hp)
            p.damage = .4f;
        else
            p.damage = std::max(0.f, p.damage - dt);
        p.last_hp = m.hp;
        bool present = battle::rectangle(ctx, i ? battle::Enemy : battle::Player, true);
        if (present && !animation) {
            auto image = battle::portrait(ctx, i ? battle::Enemy : battle::Player, m.species);
            if (image != p.image) {
                p.image = image;
                glTexSubImage2D(GL_TEXTURE_2D, 0, i * 64, 0, 56, 56, GL_RGBA, GL_UNSIGNED_BYTE,
                                p.image.data());
                p.fingerprint = battle::fingerprint(p.image);
            }
        }
        float target = (present || effect || moves) && m.hp > 0 ? 1.f : 0.f;
        p.alpha += std::clamp(target - p.alpha, -dt * 7, dt * 7);
    }
    glViewport(0, h / 3, w, h - h / 3);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(ui_theme::BattleBackground.r, ui_theme::BattleBackground.g,
                 ui_theme::BattleBackground.b, 1);
    glDepthMask(GL_TRUE);
    glClearDepthf(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program);
    glUniform2f(fade_loc, 1, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    auto matrix = camera(float(w) / (h - h / 3), double(ctx->cycles) / 4194304);
    glUniformMatrix4fv(matrix_loc, 1, GL_FALSE, matrix.data());
    glUniform1f(fog_loc, 0);
    glUniform1f(sky_loc, 0);
    glUniform1f(xray_loc, 0);
    glUniform1i(glGetUniformLocation(program, "image"), 0);
    Color arena_color = ui_theme::Grounds[terrain];
    std::vector<Vertex> v;
    quad(v, {-35, -.02f, -40}, {35, -.02f, -40}, {35, -.02f, 30}, {-35, -.02f, 30}, arena_color);
    // A restrained grid gives the original flat terrain depth without replacing
    // any collision, movement or battle logic.
    for (int z = -15; z < 12; z++)
        for (int x = -12; x < 12; x++)
            if ((x + z) & 1)
                quad(v, {float(x), 0, float(z)}, {float(x + 1), 0, float(z)},
                     {float(x + 1), 0, float(z + 1)}, {float(x), 0, float(z + 1)},
                     shade(arena_color, .97f));
    if (intro)
        platform(v, 0, -2, shade(arena_color, 1.4f));
    else {
        platform(v, 2, -3, shade(arena_color, 1.4f));
        platform(v, -2, 2, shade(arena_color, 1.4f));
    }
    for (int i = 0; i < 2; i++) {
        float x = i ? 2.f : -2.f, z = i ? -3.f : 2.f, size = i ? 2.8f : 3.1f, base = .14f;
        if (intro) {
            if (!i)
                continue;
            x = 0;
            z = -2;
            size = 4;
        }
        if (effect_kind == battle::Effect::Physical && i == effect_actor) {
            float lunge = std::max(0.f, std::sin(effect_time * 7));
            x += (i ? -1 : 1) * .9f * lunge;
            z += (i ? 1 : -1) * 1.1f * lunge;
        }
        auto &portrait = portraits[i];
        if (portrait.damage > 0)
            x += std::sin(portrait.damage * 90) * .14f;
        float alpha = overlay_alpha > 0 ? 0 : portrait.alpha;
        if (portrait.damage > 0 && int(portrait.damage * 25) % 2)
            alpha *= .3f;
        if (capture_effect && i == 1 &&
            (id == 0xc2 || id == 0xc8 || !battle::rectangle(ctx, battle::Enemy)))
            alpha = 0;
        quad(v, {x - size / 2, base + size, z}, {x + size / 2, base + size, z},
             {x + size / 2, base, z}, {x - size / 2, base, z}, {1, 1, 1, alpha},
             {float(i * 64), 0, 56, 56});
    }
    if (overlay_alpha == 0)
        effects(v, ctx);
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
    glViewport(0, 0, w, h);
    if (menu_open)
        return;
    if (overlay_alpha == 0 && !intro) {
        if (!opening || battle::name_matches(ctx, 0xcfd9, 1, 0))
            panel(ctx, true, float(w), float(h));
        if (!opening || battle::name_matches(ctx, 0xd008, 10, 7))
            panel(ctx, false, float(w), float(h));
    }
    if (intro && !unsupported_name) {
        if (styled)
            hud_text::draw(ctx->rom, ctx->rom_size, ctx->wram + 0x1049, 13, {24, 24},
                           ui_theme::HudGlyphScale, ui_theme::Ink);
        else {
            auto name = battle::text(ctx, 0xd049);
            ImGui::GetForegroundDrawList()->AddText(nullptr, 22, {24, 24}, ui_theme::BattleCaption,
                                                    name.c_str());
        }
    }
    if (framed_overlay && overlay_alpha > 0) {
        scene_filter::capture(w, h);
        scene_filter::draw(w, h);
        if (styled && (pokemon_menu::draw(ctx, w, h) || full_menu::draw_items(ctx, w, h) ||
                       full_menu::draw_naming(ctx, w, h)))
            return;
        lcd_overlay::framed(gb_get_framebuffer(ctx), float(w), float(h), overlay_alpha);
        return;
    }
    if (styled && !animation && overlay_alpha == 0 && menu.kind != battle_menu::Kind::Unknown) {
        auto drawn = menu_text::regions(ctx->wram + 0x3a0, ctx->vram, ctx->rom, ctx->rom_size,
                                        gb_get_framebuffer(ctx), menu.regions, float(w), float(h),
                                        menu_text::Placement::Battle);
        integrated_menu = true;
        menu_kind = menu.kind;
        menu_panels = drawn.panels;
        return;
    }
    lcd_overlay::draw(gb_get_framebuffer(ctx),
                      overlay_alpha > 0 ? lcd_overlay::Full : lcd_overlay::Bottom, float(w),
                      float(h), overlay_alpha > 0 ? overlay_alpha : 1, overlay_alpha == 0);
}
} // namespace battle3d
