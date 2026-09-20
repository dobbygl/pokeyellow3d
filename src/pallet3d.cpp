#include "pallet3d.h"
#include "pallet_state.h"
#include "tile_animation.h"
#include "firstperson.h"
#include "interior_scene.h"
#include "imgui.h"
#include "lcd_overlay.h"
#include "fade_state.h"
#include "menu_state.h"
#include "battle_transition_state.h"
#include "mon_pic_cache.h"
#include "pc_box_state.h"
#include "pc_details.h"
#include "dex_nests.h"
#include <SDL_opengles2.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <map>
#include <chrono>

namespace {
constexpr int AW = 512, AH = 512;
struct Vec {
    float x, y, z;
};
struct Color {
    float r, g, b, a = 1;
};
struct Vertex {
    Vec p;
    float u, v;
    Color c;
};
struct UV {
    float x, y, w, h;
};
constexpr UV Solid{511, 511, 0, 0};
constexpr Color White{1, 1, 1, 1};
GLuint program = 0, buffer = 0, atlas = 0;
GLint fade_loc = -1, matrix_loc = -1, eye_loc = -1, fog_loc = -1, sky_loc = -1, xray_loc = -1;
bool first_person = false;
firstperson::Camera eye;
firstperson::Controls controls;
GBContext *input_context = nullptr;
bool relative_allowed = false, window_focused = true;
uint8_t input_mask = 0xff;
bool dialogue_overlay = false;
int actors_drawn = 0;
bool hero_drawn = false;
bool can_compose_dialogue(const GBContext *ctx) {
    // A recognized dialogue still has an intact map behind its text box.
    // Validate that state directly so loading a dialogue savestate works even
    // before this renderer has presented an overworld frame.
    const auto *scene = ctx ? pallet::scene(pallet::read(ctx, pallet::Map)) : nullptr;
    return scene && ctx && pallet::bottom_dialogue(ctx) && pallet::valid_live_map(ctx, *scene);
}
bool enabled = true, active = false, failed = false, captured = false;
float yaw = -.32f, zoom = 1.f;
// Rooms share the outdoor default turn: an axis-aligned orthographic view hides every
// side face and reads as the flat original tilemap (see DIAGNOSTICO_LAB_OAK.md).
constexpr float RoomYaw = -.32f;
float room_yaw = RoomYaw, room_zoom = 1;
int atlas_tileset = -2;
float focus_x = 10, focus_z = 9;
bool camera_ready = false;
unsigned active_frames = 0;
std::vector<Vertex> scenery, vertices;
struct Mesh {
    GLuint buffer = 0;
    size_t count = 0;
    std::vector<uint8_t> blocks, pending;
};
std::map<int, Mesh> meshes;
std::vector<int> visible_maps;
int last_component = -1;
int preview_map = -1;
struct WorldFrame {
    int map = -1, player_first = -1, hero_begin = 0, hero_end = 0;
    firstperson::Matrix matrix{};
    Vec eye{};
    float fog = 0;
    bool fp = false;
    Vec hero{};
} world_frame;
pc_state::Motion pc_motion;
pc_state::Sample pc_sample;
pc_state::Terminal pc_terminal;
bool pc_composed = false;
bool can_compose_area(const GBContext *ctx);
bool can_compose_pc(const GBContext *ctx) {
    return preview_map < 0 && ctx && ctx->wram && world_frame.map >= 0 &&
           world_frame.map == pallet::read(ctx, pallet::Map) &&
           !pallet::read(ctx, pallet::Battle) &&
           (pc_motion.amount > 0 || pc_state::sample(ctx).mode != pc_state::Mode::None);
}
bool battle_sequence = false, battle_arena = false, battle_fighters = false, battle_dark = false,
     battle_composed = false;
battle_transition::Sample battle_phase;
bool can_compose_battle(const GBContext *ctx) {
    if (preview_map >= 0 || !battle_transition::supported(ctx))
        return false;
    if (pallet::view(ctx) == pallet::View::Battle)
        return true;
    if (world_frame.map < 0 && !battle_sequence)
        return false;
    if (battle_transition::entry(ctx) || battle::normal(ctx))
        return true;
    return battle_sequence &&
           (pallet::read(ctx, pallet::Battle) == 255 || (!battle_arena && ctx->io[0x47] == 255));
}
bool menu_overlay = false, menu_full = false, menu_blurred = false;
menu_layout::Layout menu_regions;
bool can_compose_menu(const GBContext *ctx) {
    if (!ctx || !ctx->wram || !ctx->io || preview_map >= 0 || pallet::read(ctx, pallet::Battle))
        return false;
    if (can_compose_battle(ctx))
        return false;
    const auto state = pallet::view(ctx);
    bool resident = world_frame.map >= 0 && world_frame.map == pallet::read(ctx, pallet::Map);
    if (!resident)
        return state == pallet::View::Dialogue && can_compose_dialogue(ctx);
    return state == pallet::View::Dialogue || state == pallet::View::Pokedex ||
           state == pallet::View::Computer || state == pallet::View::PokedexArea ||
           menu_state::running(ctx) || (menu_overlay && state == pallet::View::Transition);
}
bool warp_overlay = false;
int warp_destination = -1;
bool load_fade = false, load_incoming = false;
uint32_t load_started = 0;
float load_elapsed = 0;
bool can_compose_warp(const GBContext *ctx) {
    return world_frame.map >= 0 && preview_map < 0 && ctx && ctx->io && ctx->wram &&
           !pallet::read(ctx, pallet::Battle) && pallet::scene(pallet::read(ctx, pallet::Map)) &&
           fade::palette(ctx->io[0x47]) &&
           (fade::warp(ctx) || battle_transition::ending(ctx) ||
            (warp_overlay && pallet::read(ctx, pallet::Map) == warp_destination &&
             (ctx->io[0x47] == 0xff || ctx->io[0x47] == 0 || !(ctx->io[0x40] & 0x80)) &&
             pallet::view(ctx) == pallet::View::Transition));
}
const pallet::Scene *presented_scene(const GBContext *ctx) {
    return pallet::scene(preview_map >= 0 ? preview_map : pallet::read(ctx, pallet::Map));
}
size_t mesh_builds = 0;
UV tile_uv(const pallet::Scene &scene, int tile, int palette = 0) {
    int slot = scene.interior        ? 0
               : scene.tileset == 3  ? 1
               : scene.tileset == 14 ? 2
               : scene.tileset == 23 ? 3
                                     : 0;
    return {float(slot * 128 + (tile % 16) * 8), float(palette * 128 + (tile / 16) * 8), 8, 8};
}
int floor_palette(const pallet::Scene &scene, int tile) {
    if (scene.interior)
        return interior::cave(scene) && tile == 0x14 ? 2 : 0;
    if (tile == 0x14)
        return 2;
    if (scene.tileset == 0 && (tile == 0x32 || tile == 0x33 || tile == 0x54))
        return 2;
    if (scene.tileset == 14) {
        if (tile == 0x10 || tile == 0x11)
            return 2;
        return 1;
    }
    if (scene.tileset == 23 && (tile == 0x32 || tile == 0x33 || tile == 0x1f))
        return 2;
    return 0;
}
std::vector<uint8_t> pixels(AW *AH * 4);
std::array<uint8_t, AW * 32 * 4> uploaded_sprites{};
bool sprites_uploaded = false;
const Color ground[4] = {
    {.91f, .90f, .76f}, {.71f, .77f, .52f}, {.47f, .59f, .36f}, {.23f, .34f, .25f}};
const Color water[4] = {
    {.64f, .87f, .86f}, {.35f, .67f, .71f}, {.19f, .47f, .57f}, {.13f, .34f, .43f}};
const Color facade[4] = {
    {.98f, .92f, .75f}, {.73f, .79f, .64f}, {.37f, .54f, .49f}, {.18f, .29f, .30f}};

void pixel(int x, int y, Color c) {
    size_t i = (y * AW + x) * 4;
    pixels[i] = uint8_t(c.r * 255);
    pixels[i + 1] = uint8_t(c.g * 255);
    pixels[i + 2] = uint8_t(c.b * 255);
    pixels[i + 3] = uint8_t(c.a * 255);
}
Color shade(Color c, float f) {
    return {c.r * f, c.g * f, c.b * f, c.a};
}

void quad(std::vector<Vertex> &v, Vec a, Vec b, Vec c, Vec d, Color color = White, UV uv = Solid) {
    float u0 = (uv.x + .25f) / AW, v0 = (uv.y + .25f) / AH;
    float u1 = (uv.x + std::max(uv.w - .25f, .25f)) / AW;
    float v1 = (uv.y + std::max(uv.h - .25f, .25f)) / AH;
    v.insert(v.end(), {{a, u0, v0, color},
                       {b, u1, v0, color},
                       {c, u1, v1, color},
                       {a, u0, v0, color},
                       {c, u1, v1, color},
                       {d, u0, v1, color}});
}

void detailed_quad(std::vector<Vertex> &v, Vec a, Vec b, Vec c, Vec d, Color color, UV detail) {
    if (detail.w == 0) {
        quad(v, a, b, c, d, color);
        return;
    }
    // Opaque surfaces reuse the alpha channel for their atlas-cell index.
    // Repeat inside that cell in FP; keep the original solid tint in ortho.
    // This adds texture detail without any additional geometry or draw calls.
    color.a = 2 + (detail.y / 8) * (AW / 8) + detail.x / 8;
    auto length = [](Vec a, Vec b) {
        return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) +
                         (a.z - b.z) * (a.z - b.z));
    };
    float u = length(a, b) / .75f, t = length(a, d) / .75f;
    v.insert(v.end(), {{a, 0, 0, color},
                       {b, u, 0, color},
                       {c, u, t, color},
                       {a, 0, 0, color},
                       {c, u, t, color},
                       {d, 0, t, color}});
}

void box(std::vector<Vertex> &v, float x, float z, float w, float d, float bottom, float top,
         Color c, UV detail = Solid) {
    detailed_quad(v, {x, top, z}, {x + w, top, z}, {x + w, top, z + d}, {x, top, z + d},
                  shade(c, 1.08f), detail);
    detailed_quad(v, {x, top, z + d}, {x + w, top, z + d}, {x + w, bottom, z + d},
                  {x, bottom, z + d}, c, detail);
    detailed_quad(v, {x + w, top, z}, {x, top, z}, {x, bottom, z}, {x + w, bottom, z},
                  shade(c, .75f), detail);
    detailed_quad(v, {x, top, z}, {x, top, z + d}, {x, bottom, z + d}, {x, bottom, z},
                  shade(c, .85f), detail);
    detailed_quad(v, {x + w, top, z + d}, {x + w, top, z}, {x + w, bottom, z},
                  {x + w, bottom, z + d}, shade(c, .67f), detail);
}

void shadow(std::vector<Vertex> &v, float x, float z, float rx, float rz) {
    Color c{.14f, .22f, .17f, .22f};
    for (int i = 0; i < 20; i++) {
        float a = i * 6.283185f / 20, b = (i + 1) * 6.283185f / 20;
        float u = (511.25f) / AW, t = (511.25f) / AH;
        v.insert(v.end(), {{{x, .016f, z}, u, t, c},
                           {{x + std::cos(a) * rx, .016f, z + std::sin(a) * rz}, u, t, c},
                           {{x + std::cos(b) * rx, .016f, z + std::sin(b) * rz}, u, t, c}});
    }
}

void make_house(const pallet::Scene &scene, const pallet::House &h,
                const std::vector<uint8_t> &blocks) {
    const float x = h.x + scene.origin_x + .05f, z = h.z + scene.origin_z + .1f, w = h.w - .1f,
                d = h.d - .15f;
    const float eave = h.eave, ridge = eave + h.ridge;
    shadow(scenery, x + w * .5f + .3f, z + d * .65f, w * .62f, d * .65f);
    box(scenery, x, z, w, d, 0, eave, {.88f, .83f, .66f},
        tile_uv(scene, scene.tileset == 23 ? 0x30 : 0x1a, 1));
    // Original facade tiles remain individually addressable in the tileset atlas.
    int facade_rows = int((h.z + h.d) * 2) - h.facade_row;
    for (int row = 0; row < facade_rows; row++)
        for (int col = 0; col < int(h.w) * 2; col++) {
            int tile = pallet::map_tile(pallet::catalog_rom, scene, int(h.x) * 2 + col,
                                        h.facade_row + row, &blocks);
            float left = x + col * w / (h.w * 2), right = x + (col + 1) * w / (h.w * 2);
            float top = eave - row * (eave - .03f) / facade_rows,
                  bottom = eave - (row + 1) * (eave - .03f) / facade_rows;
            quad(scenery, {left, top, z + d + .006f}, {right, top, z + d + .006f},
                 {right, bottom, z + d + .006f}, {left, bottom, z + d + .006f}, White,
                 tile_uv(scene, tile, 1));
        }
    Color roof = h.lab ? Color{.28f, .48f, .53f} : Color{.69f, .31f, .23f};
    float left = x - .17f, right = x + w + .17f, back = z - .14f, front = z + d + .17f,
          mid = z + d * .5f;
    quad(scenery, {left, ridge, mid}, {right, ridge, mid}, {right, eave, front},
         {left, eave, front}, roof);
    quad(scenery, {right, ridge, mid}, {left, ridge, mid}, {left, eave, back}, {right, eave, back},
         shade(roof, .7f));
    // Gable ends and a crisp ridge cap.
    quad(scenery, {left, eave, back}, {left, ridge, mid}, {left, eave, front}, {left, eave, front},
         shade(roof, .83f));
    quad(scenery, {right, eave, front}, {right, ridge, mid}, {right, eave, back},
         {right, eave, back}, shade(roof, .62f));
    box(scenery, left, mid - .055f, right - left, .11f, ridge, ridge + .08f, shade(roof, .72f));
    for (int i = 1; i <= 5; i++) {
        float t = i / 6.f, zz = mid + (front - mid) * t, yy = ridge + (eave - ridge) * t + .012f;
        quad(scenery, {left, yy, zz}, {right, yy, zz}, {right, yy - .025f, zz + .04f},
             {left, yy - .025f, zz + .04f}, shade(roof, .82f));
    }
    box(scenery, x + w - .8f, z + .45f, .42f, .45f, eave, ridge + .25f, {.62f, .55f, .44f});
}

std::array<Color, 4> room_colors(int id) {
    std::array<Color, 4> room = {
        {{.95f, .89f, .77f}, {.74f, .70f, .56f}, {.43f, .50f, .46f}, {.16f, .23f, .25f}}};
    if (id == 6 || id == 2) {
        room[0] = {.91f, .95f, .94f};
        room[1] = {.65f, .80f, .80f};
        room[2] = {.39f, .53f, .64f};
    }
    if (id == 5 || id == 7 || id == 20 || id == 22) {
        room[0] = {.92f, .94f, .84f};
        room[1] = {.70f, .79f, .67f};
        room[2] = {.43f, .58f, .53f};
    }
    if (id == 17 || id == 11) {
        room[0] = {.72f, .72f, .66f};
        room[1] = {.48f, .52f, .49f};
        room[2] = {.31f, .37f, .35f};
    }
    return room;
}

void create_atlas(const GBContext *ctx) {
    std::fill(pixels.begin(), pixels.end(), 0);
    const auto &current = *presented_scene(ctx);
    atlas_tileset = current.interior ? current.tileset : -1;
    for (int id :
         current.interior ? std::vector<int>{current.tileset} : std::vector<int>{0, 3, 14, 23}) {
        const auto &tiles = pallet::catalog->tilesets.at(id);
        int slot = current.interior ? 0 : id == 3 ? 1 : id == 14 ? 2 : id == 23 ? 3 : 0;
        auto room = room_colors(id);
        for (int tile = 0; tile < 96; tile++)
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) {
                    size_t at = tiles.graphics + tile * 16 + y * 2;
                    int value = ((ctx->rom[at] >> (7 - x)) & 1) |
                                (((ctx->rom[at + 1] >> (7 - x)) & 1) << 1);
                    for (int palette = 0; palette < 3; palette++)
                        pixel(slot * 128 + (tile % 16) * 8 + x, palette * 128 + (tile / 16) * 8 + y,
                              (current.interior ? (palette == 2 ? water : room.data())
                                                : (palette == 0   ? ground
                                                   : palette == 1 ? facade
                                                                  : water))[value]);
                }
    }
    pixel(511, 511, White);
    sprites_uploaded = false;
}

PalletTileAnimationInfo tile_animation_info{};
void animate_tiles(const GBContext *ctx, const pallet::Scene &current) {
    auto phase = preview_map < 0 ? tile_animation::sample(ctx, pallet::tileset(current))
                                 : tile_animation::Phase{};
    tile_animation_info.water_shift = phase.water;
    tile_animation_info.flower_frame = phase.flower;
    tile_animation_info.texture = atlas;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas);
    for (int id :
         current.interior ? std::vector<int>{current.tileset} : std::vector<int>{0, 3, 14, 23}) {
        const auto &tiles = pallet::catalog->tilesets.at(id);
        int slot = current.interior ? 0 : id == 3 ? 1 : id == 14 ? 2 : id == 23 ? 3 : 0;
        auto room = room_colors(id);
        for (int tile : {tile_animation::Water, tile_animation::Flower}) {
            auto bytes = tile_animation::frame(ctx, tiles, tile, phase);
            for (int palette = 0; palette < 3; ++palette) {
                const auto *colors = current.interior ? (palette == 2 ? water : room.data())
                                     : palette == 0   ? ground
                                     : palette == 1   ? facade
                                                      : water;
                int ox = slot * 128 + (tile % 16) * 8;
                int oy = palette * 128 + (tile / 16) * 8;
                std::array<uint8_t, 8 * 8 * 4> patch{};
                bool changed = false;
                for (int y = 0; y < 8; ++y)
                    for (int x = 0; x < 8; ++x) {
                        int value = ((bytes[y * 2] >> (7 - x)) & 1) |
                                    (((bytes[y * 2 + 1] >> (7 - x)) & 1) << 1);
                        const auto &c = colors[value];
                        std::array<uint8_t, 4> rgba{uint8_t(c.r * 255), uint8_t(c.g * 255),
                                                    uint8_t(c.b * 255), uint8_t(c.a * 255)};
                        auto *dest = pixels.data() + ((oy + y) * AW + ox + x) * 4;
                        changed |= std::memcmp(dest, rgba.data(), 4) != 0;
                        std::copy(rgba.begin(), rgba.end(), dest);
                        std::copy(rgba.begin(), rgba.end(), patch.begin() + (y * 8 + x) * 4);
                    }
                if (changed) {
                    glTexSubImage2D(GL_TEXTURE_2D, 0, ox, oy, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE,
                                    patch.data());
                    ++tile_animation_info.uploads;
                }
            }
        }
    }
}

void interior_map(const GBContext *ctx, const pallet::Scene &scene,
                  const std::vector<uint8_t> &blocks) {
    for (int z = 0; z < scene.height; z++)
        for (int x = 0; x < scene.width; x++) {
            auto cell = interior::classify(ctx->rom, scene, x, z, &blocks);
            if (cell.height <= 0)
                continue;
            int tile = pallet::map_tile(ctx->rom, scene, x * 2, z * 2, &blocks);
            UV uv = tile_uv(scene, tile);
            Color side{};
            side.r = side.g = side.b = 0;
            side.a = 1;
            for (int y = 0; y < 8; y++)
                for (int xx = 0; xx < 8; xx++) {
                    size_t at = ((int(uv.y) + y) * AW + int(uv.x) + xx) * 4;
                    side.r += pixels[at] / (255.f * 64);
                    side.g += pixels[at + 1] / (255.f * 64);
                    side.b += pixels[at + 2] / (255.f * 64);
                }
            box(scenery, float(x), float(z), 1, 1, 0, cell.height, side);
            for (int row = 0; row < 2; row++)
                for (int col = 0; col < 2; col++) {
                    int t = pallet::map_tile(ctx->rom, scene, x * 2 + col, z * 2 + row, &blocks);
                    float left = x + col * .5f, back = z + row * .5f, h = cell.height + .002f;
                    quad(scenery, {left, h, back}, {left + .5f, h, back},
                         {left + .5f, h, back + .5f}, {left, h, back + .5f}, White,
                         tile_uv(scene, t));
                    if (cell.kind == interior::Kind::Wall || cell.kind == interior::Kind::Counter) {
                        float top = cell.height - row * cell.height * .5f,
                              bottom = top - cell.height * .5f;
                        quad(scenery, {left, top, z + 1.002f}, {left + .5f, top, z + 1.002f},
                             {left + .5f, bottom, z + 1.002f}, {left, bottom, z + 1.002f}, White,
                             tile_uv(scene, t));
                    }
                }
        }
}

void create_map(const GBContext *ctx, const pallet::Scene &scene,
                const std::vector<uint8_t> &blocks) {
    scenery.clear();
    for (int tz = 0; tz < scene.height * 2; tz++)
        for (int tx = 0; tx < scene.width * 2; tx++) {
            int tile = pallet::map_tile(ctx->rom, scene, tx, tz, &blocks);
            if (!scene.interior && pallet::cleared(ctx->rom, scene, tx / 2, tz / 2, &blocks))
                tile = scene.tileset == 3 ? 0x30 : scene.tileset == 23 ? 0x23 : 0x2c;
            float x = scene.origin_x + tx * .5f, z = scene.origin_z + tz * .5f;
            quad(scenery, {x, 0, z}, {x + .5f, 0, z}, {x + .5f, 0, z + .5f}, {x, 0, z + .5f}, White,
                 tile_uv(scene, tile, floor_palette(scene, tile)));
        }
    if (scene.interior) {
        interior_map(ctx, scene, blocks);
        box(scenery, 0, 0, float(scene.width), float(scene.height), -.35f, -.015f,
            {.32f, .36f, .34f});
        return;
    }
    for (auto h : pallet::houses(scene))
        make_house(scene, h, blocks);
    for (int iz = 0; iz < scene.height; iz++)
        for (int ix = 0; ix < scene.width; ix++) {
            float x = ix + scene.origin_x, z = iz + scene.origin_z;
            auto type = pallet::terrain(ctx->rom, scene, ix, iz, &blocks);
            UV foliage = tile_uv(scene, scene.tileset == 3 ? 0x16 : 0x41);
            UV stone = tile_uv(scene, scene.tileset == 23 ? 0x28 : 0x3a);
            if (type == pallet::Terrain::TallTree) {
                shadow(scenery, x + 1, z + 1.2f, 1.f, .75f);
                box(scenery, x + .8f, z + 1.f, .4f, .4f, 0, 1.f, {.40f, .31f, .20f});
                box(scenery, x + .1f, z + .15f, 1.8f, 1.7f, .8f, 1.8f, {.22f, .40f, .27f}, foliage);
                box(scenery, x + .3f, z + .3f, 1.4f, 1.4f, 1.8f, 2.35f, {.30f, .49f, .30f},
                    foliage);
                box(scenery, x + .5f, z + .5f, 1.f, 1.f, 2.35f, 2.65f, {.38f, .57f, .35f}, foliage);
            } else if (type == pallet::Terrain::Portal) {
                box(scenery, x - .1f, z + .65f, .18f, .35f, 0, 1.7f, {.65f, .66f, .54f});
                box(scenery, x + .92f, z + .65f, .18f, .35f, 0, 1.7f, {.65f, .66f, .54f});
                box(scenery, x - .1f, z + .65f, 1.2f, .35f, 1.7f, 1.9f, {.77f, .78f, .63f});
                for (int row = 0; row < 2; row++)
                    for (int col = 0; col < 2; col++) {
                        int tile =
                            pallet::map_tile(ctx->rom, scene, ix * 2 + col, iz * 2 + row, &blocks);
                        float left = x + col * .5f, top = 1.7f - row * .85f;
                        quad(scenery, {left, top, z + 1}, {left + .5f, top, z + 1},
                             {left + .5f, top - .85f, z + 1}, {left, top - .85f, z + 1}, White,
                             tile_uv(scene, tile, 1));
                    }
            } else if (type == pallet::Terrain::Pillar) {
                box(scenery, x + .16f, z + .16f, .68f, .68f, 0, 1.45f, {.71f, .73f, .59f});
                box(scenery, x + .05f, z + .05f, .9f, .9f, 1.45f, 1.65f, {.83f, .82f, .67f});
            } else if (type == pallet::Terrain::Wall) {
                box(scenery, x, z, 1, 1, 0, 1.15f, {.64f, .66f, .53f});
            } else if (type == pallet::Terrain::Tree || type == pallet::Terrain::CutTree) {
                shadow(scenery, x + .6f, z + .6f, .55f, .42f);
                box(scenery, x + .4f, z + .4f, .2f, .2f, 0, .65f, {.40f, .31f, .20f});
                box(scenery, x + .08f, z + .12f, .84f, .78f, .5f, 1.10f, {.29f, .48f, .30f},
                    foliage);
                box(scenery, x + .18f, z + .20f, .64f, .60f, 1.10f, 1.50f, {.38f, .56f, .34f},
                    foliage);
                box(scenery, x + .29f, z + .30f, .42f, .4f, 1.50f, 1.68f, {.47f, .63f, .39f},
                    foliage);
            } else if (type == pallet::Terrain::Rock) {
                box(scenery, x + .15f, z + .18f, .70f, .64f, 0, .45f, {.49f, .53f, .42f}, stone);
                box(scenery, x + .24f, z + .26f, .52f, .48f, .45f, .61f, {.63f, .65f, .51f}, stone);
            } else if (type == pallet::Terrain::Ledge) {
                int bottom = pallet::map_tile(ctx->rom, scene, ix * 2, iz * 2 + 1, &blocks);
                if (bottom == 0x27)
                    box(scenery, x, z, .24f, 1, 0, .3f, {.57f, .47f, .30f});
                else if (bottom == 0x0d || bottom == 0x1d)
                    box(scenery, x + .76f, z, .24f, 1, 0, .3f, {.57f, .47f, .30f});
                else {
                    box(scenery, x, z + .72f, 1, .25f, 0, .27f, {.57f, .47f, .30f});
                    box(scenery, x, z + .70f, 1, .24f, .27f, .32f, {.56f, .65f, .39f});
                }
            } else if (type == pallet::Terrain::Grass) {
                for (int i = 0; i < 4; i++) {
                    float px = x + .18f + (i % 2) * .48f, pz = z + .22f + (i / 2) * .46f;
                    quad(scenery, {px - .08f, .02f, pz}, {px, .26f, pz}, {px + .09f, .02f, pz},
                         {px + .09f, .02f, pz}, {.32f, .51f, .26f});
                    quad(scenery, {px, .02f, pz - .08f}, {px, .21f, pz}, {px, .02f, pz + .09f},
                         {px, .02f, pz + .09f}, {.42f, .60f, .30f});
                }
            } else if (type == pallet::Terrain::Fence) {
                for (float off : {.16f, .66f})
                    box(scenery, x + off, z + .55f, .12f, .14f, 0, .65f, {.90f, .87f, .70f});
                box(scenery, x, z + .57f, 1, .10f, .25f, .36f, {.75f, .73f, .58f});
            } else if (type == pallet::Terrain::Sign) {
                box(scenery, x + .43f, z + .45f, .14f, .12f, 0, .5f, {.48f, .35f, .21f});
                box(scenery, x + .1f, z + .4f, .8f, .16f, .45f, .98f, {.58f, .42f, .27f});
                for (int row = 0; row < 2; row++)
                    for (int col = 0; col < 2; col++) {
                        int tile =
                            pallet::map_tile(ctx->rom, scene, ix * 2 + col, iz * 2 + row, &blocks);
                        float left = x + .1f + col * .4f, top = .98f - row * .265f;
                        quad(scenery, {left, top, z + .566f}, {left + .4f, top, z + .566f},
                             {left + .4f, top - .265f, z + .566f}, {left, top - .265f, z + .566f},
                             White, tile_uv(scene, tile, 1));
                    }
            }
        }
    box(scenery, scene.origin_x, scene.origin_z, float(scene.width), float(scene.height), -.8f,
        -.015f, {.42f, .48f, .34f});
}

void update_meshes(const GBContext *ctx) {
    const auto &current = *presented_scene(ctx);
    visible_maps = preview_map >= 0 ? std::vector<int>{current.id} : pallet::resident_maps(current);
    if (last_component != current.component) {
        camera_ready = false;
        eye.reset();
        room_yaw = RoomYaw;
        room_zoom = 1;
    }
    last_component = current.component;
    for (auto it = meshes.begin(); it != meshes.end();) {
        if (std::find(visible_maps.begin(), visible_maps.end(), it->first) == visible_maps.end()) {
            glDeleteBuffers(1, &it->second.buffer);
            it = meshes.erase(it);
        } else
            ++it;
    }
    for (int id : visible_maps) {
        const auto &scene = *pallet::scene(id);
        auto blocks = scene.block_data;
        // Runtime map has a three-block border on every side (wOverworldMap).
        if (id == current.id && preview_map < 0) {
            int bw = scene.width / 2, bh = scene.height / 2;
            for (int z = 0; z < bh; z++)
                for (int x = 0; x < bw; x++)
                    blocks[z * bw + x] =
                        pallet::read(ctx, uint16_t(0xc6e8 + (z + 3) * (bw + 6) + x + 3));
        }
        auto &mesh = meshes[id];
        if (id == current.id && preview_map < 0 && blocks != mesh.pending) {
            mesh.pending = blocks;
            // Header/coordinates can switch one frame before the padded map buffer.
            // Keep the previous valid mesh until the incoming buffer is stable.
            blocks = mesh.buffer ? mesh.blocks : scene.block_data;
        }
        if (mesh.buffer && mesh.blocks == blocks)
            continue;
        auto start = std::chrono::steady_clock::now();
        create_map(ctx, scene, blocks);
        if (!mesh.buffer)
            glGenBuffers(1, &mesh.buffer);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.buffer);
        glBufferData(GL_ARRAY_BUFFER, scenery.size() * sizeof(Vertex), scenery.data(),
                     GL_STATIC_DRAW);
        mesh.count = scenery.size();
        mesh.blocks = std::move(blocks);
        ++mesh_builds;
        double ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();
        std::fprintf(stderr, "[3D] mesh map=%d vertices=%zu bytes=%zu build=%.2fms resident=%zu\n",
                     id, mesh.count, mesh.count * sizeof(Vertex), ms, meshes.size());
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

GLuint shader(GLenum type, const char *source) {
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);
    GLint ok;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(id, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[3D] shader: %s\n", log);
        glDeleteShader(id);
        return 0;
    }
    return id;
}

#include "scene_filter.h"
#include "presentation_blend_gl.h"
const uint32_t *presented_lcd = nullptr;
bool wants_scene(const GBContext *ctx) {
    if (!enabled || failed)
        return false;
    auto state = pallet::view(ctx);
    return (preview_map >= 0 && presented_scene(ctx)) || state == pallet::View::Overworld ||
           state == pallet::View::Battle || state == pallet::View::Pokedex || load_fade ||
           can_compose_area(ctx) || can_compose_menu(ctx) || can_compose_warp(ctx) ||
           can_compose_battle(ctx) || can_compose_pc(ctx);
}
bool frozen_blend(const GBContext *ctx) {
    return presentation::blend.paused && presentation::history.valid &&
           (presentation::blend.running || wants_scene(ctx) != presentation::blend.target);
}
void original_frame(GBContext *ctx, int w, int h) {
    glViewport(0, 0, w, h);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    lcd_overlay::draw(presented_lcd ? presented_lcd : gb_get_framebuffer(ctx), lcd_overlay::Full,
                      float(w), float(h));
}

bool initialize(const GBContext *ctx) {
    const char *vs = R"(
        attribute vec3 position; attribute vec2 texcoord; attribute vec4 color;
        uniform mat4 view_projection; uniform vec3 eye;
        uniform mediump float fog_enabled; uniform mediump float sky;
        varying vec2 uv; varying vec4 tint; varying float distance_to_eye;
        void main() {
            gl_Position=sky>0.5?vec4(position.xy,0.999,1.0):view_projection*vec4(position,1.0);
            distance_to_eye=fog_enabled>0.5?length(position-eye):0.0;
            uv=texcoord; tint=color;
            // Resolve flat materials once per vertex for the orthographic view.
            // Its fragment path remains a single texture lookup, as before FP.
            if(fog_enabled<0.5 && color.a>1.5) {uv=vec2(511.25/512.0);tint.a=1.0;}
            if(sky>0.5)uv=position.xy*0.5+0.5;
        })";
    const char *fs = R"(
        precision mediump float;
        uniform vec2 fade_tone;
        uniform sampler2D image; uniform float xray; uniform mediump float fog_enabled; uniform mediump float sky;
        varying vec2 uv; varying vec4 tint; varying float distance_to_eye;
        void main() {
            vec4 c;
            if(fog_enabled<0.5) {
                c=texture2D(image,uv)*tint;
            } else {
                if(sky>0.5) {
                    if(fog_enabled>1.5) {gl_FragColor=vec4(vec3(0.08,0.10,0.12)*fade_tone.x+fade_tone.y,1.0);return;}
                    gl_FragColor=vec4(mix(vec3(0.70,0.80,0.75),vec3(0.23,0.42,0.55),smoothstep(0.4,1.0,uv.y))*fade_tone.x+fade_tone.y,1.0);
                    return;
                }
                if(tint.a>1.5) {
                    c=vec4(tint.rgb,1.0);
                    float cell=floor(tint.a-2.0+0.1);
                    vec2 base=vec2(mod(cell,64.0),floor(cell/64.0))*8.0;
                    c*=texture2D(image,(base+vec2(0.25)+fract(uv)*7.5)/512.0);
                }
                else c=texture2D(image,uv)*tint;
                if(fog_enabled>1.5)c.rgb=mix(c.rgb,vec3(0.08,0.10,0.12),smoothstep(4.0,18.0,distance_to_eye));
                else c.rgb=mix(c.rgb,vec3(0.68,0.79,0.75),smoothstep(18.0,70.0,distance_to_eye));
            }
            if(c.a<0.08) discard;
            if(xray>0.5) { c.rgb=vec3(0.98,0.80,0.29); c.a*=0.7; }
            c.rgb=c.rgb*fade_tone.x+fade_tone.y;
            gl_FragColor=c;
        })";
    GLuint a = shader(GL_VERTEX_SHADER, vs), b = shader(GL_FRAGMENT_SHADER, fs);
    if (!a || !b) {
        if (a)
            glDeleteShader(a);
        if (b)
            glDeleteShader(b);
        return false;
    }
    program = glCreateProgram();
    glAttachShader(program, a);
    glAttachShader(program, b);
    glBindAttribLocation(program, 0, "position");
    glBindAttribLocation(program, 1, "texcoord");
    glBindAttribLocation(program, 2, "color");
    glLinkProgram(program);
    glDeleteShader(a);
    glDeleteShader(b);
    GLint ok;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[3D] shader link: %s\n", log);
        return false;
    }
    matrix_loc = glGetUniformLocation(program, "view_projection");
    eye_loc = glGetUniformLocation(program, "eye");
    fog_loc = glGetUniformLocation(program, "fog_enabled");
    sky_loc = glGetUniformLocation(program, "sky");
    xray_loc = glGetUniformLocation(program, "xray");
    fade_loc = glGetUniformLocation(program, "fade_tone");
    glGenBuffers(1, &buffer);
    glGenTextures(1, &atlas);
    glBindTexture(GL_TEXTURE_2D, atlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    create_atlas(ctx);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, AW, AH, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    std::fprintf(stderr, "[3D] ROM catalog: %zu scenes; tileset atlas ready.\n",
                 pallet::catalog->maps.size());
    return true;
}

#include "battle3d.h"
#include "dex3d.h"
#include "dex_area3d.h"
bool can_compose_area(const GBContext *ctx) {
    return preview_map < 0 && dex_area3d::wants(ctx);
}

void sprite_image(const GBContext *ctx, const pallet::Actor &actor) {
    int ox = actor.slot * 32, oy = 384;
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++)
            pixel(ox + x, oy + y, {0, 0, 0, 0});
    int image = actor.image, frame = image < 0xa0 ? (image & 15) : 0;
    size_t table = pallet::FacingTable + frame * 2;
    size_t at = ctx->rom[table] | (ctx->rom[table + 1] << 8);
    int count = ctx->rom[at++];
    if (count > 9 || at + count * 4 >= ctx->rom_size)
        return;
    int base = image >> 4;
    base = base == 11 ? 124 : base * 12;
    Color colors[4] = {{0, 0, 0, 0}, {.97f, .91f, .73f}, {.75f, .31f, .25f}, {.16f, .20f, .22f}};
    if (actor.slot == 15) {
        colors[1] = {1, .88f, .27f};
        colors[2] = {.77f, .47f, .15f};
    } else if (actor.slot != 0)
        colors[2] = {.31f, .51f, .57f};
    for (int i = 0; i < count; i++, at += 4) {
        int dy = int8_t(ctx->rom[at]), dx = int8_t(ctx->rom[at + 1]);
        int tile = (base + ctx->rom[at + 2]) & 255, flags = ctx->rom[at + 3];
        if (tile >= 128)
            tile = (tile + ctx->hram[0x7c]) & 255;
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) {
                int tx = (flags & 0x20) ? 7 - x : x, ty = (flags & 0x40) ? 7 - y : y;
                int value = ((ctx->vram[tile * 16 + ty * 2] >> (7 - tx)) & 1) |
                            (((ctx->vram[tile * 16 + ty * 2 + 1] >> (7 - tx)) & 1) << 1);
                int px = 8 + dx + x, py = 8 + dy + y;
                if (px >= 0 && px < 32 && py >= 0 && py < 32)
                    pixel(ox + px, oy + py, colors[value]);
            }
    }
}

void draw_world_frame(int w, int h, fade::Tone tone = {}, bool hide_hero = false,
                      bool allow_xray = true, float approach = 0,
                      const firstperson::Matrix *override_matrix = nullptr);

void world(GBContext *ctx, int w, int h, fade::Tone tone = {}) {
    const auto &current = *presented_scene(ctx);
    int needed_atlas = current.interior ? current.tileset : -1;
    if (atlas_tileset != needed_atlas) {
        create_atlas(ctx);
        glBindTexture(GL_TEXTURE_2D, atlas);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, AW, AH, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    }
    animate_tiles(ctx, current);
    update_meshes(ctx);
    vertices.clear();
    const bool fp = first_person && preview_map < 0;
    const auto player_position = pallet::world_player(ctx);
    float jump = 0;
    if ((pallet::read(ctx, 0xd735) & 0x40) && pallet::read(ctx, 0xd713) > 0)
        jump = .14f *
               std::sin(std::clamp(int(pallet::read(ctx, 0xd713)), 0, 15) * firstperson::Pi / 15);
    if (fp && (!dialogue_overlay || !eye.ready))
        eye.update(player_position[0], player_position[1], pallet::read(ctx, 0xc109),
                   ImGui::GetIO().DeltaTime, ctx->cycles, jump);
    int player_first = -1, hero_begin = 0, hero_end = 0;
    actors_drawn = 0;
    hero_drawn = false;
    float view_yaw = current.interior ? room_yaw : yaw;
    float cs = std::cos(view_yaw), sn = std::sin(view_yaw);
    for (int slot = 0; preview_map < 0 && slot < 16; slot++) {
        if (fp && slot == 0)
            continue;
        pallet::Actor a;
        if (!pallet::actor(ctx, slot, a))
            continue;
        sprite_image(ctx, a);
        ++actors_drawn;
        hero_drawn |= slot == 0;
        const auto *scene = pallet::scene(pallet::read(ctx, pallet::Map));
        a.x += scene->origin_x;
        a.z += scene->origin_z;
        if (slot == 0)
            hero_begin = int(vertices.size());
        shadow(vertices, a.x, a.z, .32f, .20f);
        float half = .95f; // Transparent padding makes the visible sprite one tile wide.
        // Face the camera in both axes, preserving the original sprite aspect.
        auto corner = [&](float side, float up) -> Vec {
            return {a.x + cs * side - sn * .78f * up, .6257795f * up,
                    a.z - sn * side - cs * .78f * up};
        };
        Vec tl = corner(-half, 1.425f), tr = corner(half, 1.425f);
        Vec br = corner(half, -.475f), bl = corner(-half, -.475f);
        if (fp) {
            float dx = eye.x - a.x, dz = eye.z - a.z, len = std::hypot(dx, dz);
            float rx = len > .001f ? -dz / len : std::cos(eye.yaw),
                  rz = len > .001f ? dx / len : std::sin(eye.yaw);
            int bottom = 0;
            for (int y = 0; y < 32; y++)
                for (int x = 0; x < 32; x++)
                    if (pixels[((384 + y) * AW + slot * 32 + x) * 4 + 3])
                        bottom = y + 1;
            float top = bottom * (1.9f / 32), low = top - 1.9f;
            tl = {a.x - rx * half, top, a.z - rz * half};
            tr = {a.x + rx * half, top, a.z + rz * half};
            bl = {a.x - rx * half, low, a.z - rz * half};
            br = {a.x + rx * half, low, a.z + rz * half};
        }
        if (slot == 0)
            player_first = int(vertices.size());
        quad(vertices, tl, tr, br, bl, White, {float(slot * 32), 384, 32, 32});
        if (slot == 0)
            hero_end = int(vertices.size());
    }
    if (fp && current.interior) {
        // Close the room for the eye-level view; the orthographic camera keeps
        // its open-top cutaway. The shell is outside all playable map cells.
        Color wall = interior::cave(current) ? Color{.25f, .30f, .28f} : Color{.68f, .68f, .59f};
        UV texture = tile_uv(current, pallet::map_tile(ctx->rom, current, current.width, 0));
        box(vertices, -.12f, 0, .12f, current.height, 0, 2.6f, wall, texture);
        box(vertices, current.width, 0, .12f, current.height, 0, 2.6f, wall, texture);
        box(vertices, -.12f, -.12f, current.width + .24f, .12f, 0, 2.6f, wall, texture);
        box(vertices, -.12f, current.height, current.width + .24f, .12f, 0, 2.6f, wall, texture);
        quad(vertices, {0, 2.6f, 0}, {float(current.width), 2.6f, 0},
             {float(current.width), 2.6f, float(current.height)}, {0, 2.6f, float(current.height)},
             shade(wall, .65f));
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas);
    // Avoid a texture transfer (and its GPU synchronization) when sprites have
    // not changed. Compare actual decoded pixels so VRAM and state reloads count.
    const auto *sprites = pixels.data() + 384 * AW * 4;
    if (!sprites_uploaded ||
        std::memcmp(uploaded_sprites.data(), sprites, uploaded_sprites.size())) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 384, AW, 32, GL_RGBA, GL_UNSIGNED_BYTE, sprites);
        std::memcpy(uploaded_sprites.data(), sprites, uploaded_sprites.size());
        sprites_uploaded = true;
    }
    const auto p = preview_map >= 0 ? std::array<float, 2>{current.origin_x + current.width * .5f,
                                                           current.origin_z + current.height * .5f}
                                    : pallet::world_player(ctx);
    auto target = pallet::camera_target(p[0], p[1], zoom);
    if (current.interior) {
        float follow = std::clamp(room_zoom - 1.f, 0.f, 1.f);
        target = {current.width * .5f + (p[0] - current.width * .5f) * follow,
                  current.height * .5f + (p[1] - current.height * .5f) * follow};
    }
    if (!camera_ready || std::abs(target[0] - focus_x) + std::abs(target[1] - focus_z) > 8) {
        focus_x = target[0];
        focus_z = target[1];
        camera_ready = true;
    } else if (!dialogue_overlay) {
        float amount = 1 - std::exp(-8.f * std::min(ImGui::GetIO().DeltaTime, .1f));
        focus_x += (target[0] - focus_x) * amount;
        focus_z += (target[1] - focus_z) * amount;
    }

    float unit = std::min(w / 27.f, h / 22.f) * zoom;
    if (preview_map >= 0 || current.interior)
        unit = std::min(
            w / (current.width * std::abs(cs) + current.height * std::abs(sn) + 5),
            h / ((current.width * std::abs(sn) + current.height * std::abs(cs)) * .78f + 6));
    if (current.interior && preview_map < 0)
        unit *= room_zoom;
    auto matrix =
        fp ? eye.perspective(float(w) / h)
           : firstperson::orthographic(focus_x, focus_z, view_yaw, 2 * unit / w, 2 * unit / h);
    world_frame = {current.id,
                   player_first,
                   hero_begin,
                   hero_end,
                   matrix,
                   {eye.x, eye.y, eye.z},
                   float(fp ? (current.interior && interior::cave(current) ? 2 : 1) : 0),
                   fp,
                   {player_position[0], .6f, player_position[1]}};
    draw_world_frame(w, h, tone);
}

// Pure presentation of resident meshes/actors and the last valid camera.
// In particular this never reads the incoming map's WRAM or decodes its VRAM.
std::array<float, 2> battle_zoom_center() {
    if (world_frame.fp)
        return {0, 0};
    const auto &m = world_frame.matrix;
    const auto p = world_frame.hero;
    float denominator = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
    if (std::abs(denominator) < .001f)
        return {0, 0};
    return {(m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12]) / denominator,
            (m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13]) / denominator};
}
void draw_world_frame(int w, int h, fade::Tone tone, bool hide_hero, bool allow_xray,
                      float approach, const firstperson::Matrix *override_matrix) {
    const auto &frame = world_frame;
    glViewport(0, 0, w, h);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(.075f * tone.multiply + tone.add, .12f * tone.multiply + tone.add,
                 .135f * tone.multiply + tone.add, 1);
    glDepthMask(GL_TRUE);
    glClearDepthf(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas);
    glUniform2f(fade_loc, tone.multiply, tone.add);
    auto matrix = override_matrix ? *override_matrix : frame.matrix;
    if (approach > 0) {
        auto center = battle_zoom_center();
        float scale = 1 + approach * 1.6f;
        for (int col = 0; col < 4; col++)
            for (int row = 0; row < 2; row++)
                matrix[col * 4 + row] = scale * frame.matrix[col * 4 + row] -
                                        (scale - 1) * center[row] * frame.matrix[col * 4 + 3];
    }
    glUniformMatrix4fv(matrix_loc, 1, GL_FALSE, matrix.data());
    glUniform3f(eye_loc, frame.eye.x, frame.eye.y, frame.eye.z);
    glUniform1f(fog_loc, frame.fog);
    glUniform1f(sky_loc, 0);
    glUniform1i(glGetUniformLocation(program, "image"), 0);
    glUniform1f(xray_loc, 0);
    for (int i = 0; i < 3; i++)
        glEnableVertexAttribArray(i);
    auto bind_vertices = [](GLuint id) {
        glBindBuffer(GL_ARRAY_BUFFER, id);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void *)offsetof(Vertex, p));
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void *)offsetof(Vertex, u));
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void *)offsetof(Vertex, c));
    };
    if (frame.fp) {
        std::vector<Vertex> sky;
        quad(sky, {-1, 1, 0}, {1, 1, 0}, {1, -1, 0}, {-1, -1, 0});
        bind_vertices(buffer);
        glBufferData(GL_ARRAY_BUFFER, sky.size() * sizeof(Vertex), sky.data(), GL_STREAM_DRAW);
        glDisable(GL_DEPTH_TEST);
        glUniform1f(sky_loc, 1);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEnable(GL_DEPTH_TEST);
        glUniform1f(sky_loc, 0);
    }
    for (int id : visible_maps) {
        const auto &mesh = meshes.at(id);
        bind_vertices(mesh.buffer);
        glDrawArrays(GL_TRIANGLES, 0, GLsizei(mesh.count));
    }
    bind_vertices(buffer);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(),
                 GL_STREAM_DRAW);
    if (hide_hero && frame.hero_end > frame.hero_begin) {
        glDrawArrays(GL_TRIANGLES, 0, frame.hero_begin);
        glDrawArrays(GL_TRIANGLES, frame.hero_end, GLsizei(vertices.size()) - frame.hero_end);
    } else
        glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));
    // Keep the player locatable when a roof obscures the camera's line of sight.
    if (frame.player_first >= 0 && !hide_hero && allow_xray) {
        glDepthFunc(GL_GREATER);
        glDepthMask(GL_FALSE);
        glUniform1f(xray_loc, 1);
        glDrawArrays(GL_TRIANGLES, frame.player_first, 6);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
        glUniform1f(xray_loc, 0);
    }
    for (int i = 0; i < 3; i++)
        glDisableVertexAttribArray(i);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
}

#include "pc_boxes.h"
#include "pc_hall3d.h"
#include "pc3d.h"

void hud(const GBContext *ctx) {
    ImGuiIO &io = ImGui::GetIO();
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    float s = std::max(1.f, io.DisplaySize.x / 1000.f);
    const ImU32 dim = IM_COL32(159, 186, 177, 255), bright = IM_COL32(241, 237, 215, 255);
    dl->AddRectFilledMultiColor({0, 0}, {io.DisplaySize.x, 113 * s}, IM_COL32(13, 25, 28, 235),
                                IM_COL32(13, 25, 28, 235), IM_COL32(13, 25, 28, 0),
                                IM_COL32(13, 25, 28, 0));
    dl->AddRectFilledMultiColor({0, io.DisplaySize.y - 65 * s}, io.DisplaySize,
                                IM_COL32(13, 25, 28, 0), IM_COL32(13, 25, 28, 0),
                                IM_COL32(13, 25, 28, 245), IM_COL32(13, 25, 28, 245));
    dl->AddText(nullptr, 13 * s, {25 * s, 22 * s}, dim, "KANTO  /  01");
    dl->AddText(nullptr, 26 * s, {25 * s, 43 * s}, bright, presented_scene(ctx)->title.c_str());
    dl->AddText(nullptr, 13 * s, {25 * s, 78 * s}, dim,
                first_person ? "Amarillo  /  Primera persona" : "Amarillo  /  Prototipo 3D");
    dl->AddText(nullptr, 13 * s, {25 * s, io.DisplaySize.y - 42 * s}, bright,
                first_person
                    ? "F2  2D / 3D     F3  Vista ortografica"
                    : "F2  2D / 3D     F3  Primera persona     Q / E  Girar     Rueda  Zoom");
    dl->AddText(nullptr, 12 * s, {25 * s, io.DisplaySize.y - 23 * s}, dim,
                first_person
                    ? "W  Avanzar   A / D  Girar   S  Media vuelta   Z  Hablar   Enter  Menu"
                    : "Flechas / WASD  Mover     Z  Hablar     Enter  Menu     Esc  Ajustes");
}
} // namespace

void pallet3d_draw(GBContext *ctx, int width, int height, bool menu_open) {
    dex3d::presented = false;
    dex_area3d::presented = false;
    dex_area3d::synchronize(ctx);
    pc3d::presented = false;
    pc_boxes::presented = false;
    pc_hall3d::presented = false;
    if (frozen_blend(ctx)) {
        active = presentation::draw(presentation::history, width, height);
        return;
    }
    auto state = pallet::view(ctx);
    if (!ctx || !ctx->wram || world_frame.map != pallet::read(ctx, pallet::Map) ||
        (pc_motion.initialized && uint32_t(ctx->cycles) < pc_motion.last &&
         pc_motion.last - uint32_t(ctx->cycles) < 0x80000000u))
        pc3d::reset();
    pc_sample = pc_state::sample(ctx);
    bool pc_wanted = enabled && !failed && preview_map < 0 && world_frame.map >= 0 &&
                     pc_sample.terminal.map == world_frame.map;
    if (pc_wanted)
        pc_terminal = pc_sample.terminal;
    pc_motion.update(pc_wanted, ctx ? uint32_t(ctx->cycles) : 0, menu_open || !window_focused);
    pc_composed = enabled && !failed && preview_map < 0 && (pc_wanted || pc_motion.amount > 0);
    battle_composed = can_compose_battle(ctx);
    battle_phase = battle_transition::sample(ctx, ctx ? gb_get_framebuffer(ctx) : nullptr);
    if (battle_composed) {
        if (!battle_sequence) {
            battle_arena = battle_fighters = battle_dark = false;
            battle3d::reset();
        }
        battle_sequence = true;
        battle_arena |= battle_phase.hud || state == pallet::View::Battle;
        battle_fighters |= state == pallet::View::Battle && !battle::trainer_intro(ctx);
        battle_dark |= battle_phase.phase == battle_transition::Phase::Loading;
    } else {
        battle_sequence = battle_arena = battle_fighters = battle_dark = false;
    }
    if (std::getenv("PALLET3D_TRACE") && ctx && ctx->wram) {
        static int last = -1, lastx = -1, lasty = -1;
        int x = pallet::read(ctx, pallet::X), y = pallet::read(ctx, pallet::Y);
        if (last != int(state) || x != lastx || y != lasty) {
            std::fprintf(stderr, "[3D] state=%d map=%u xy=%d,%d font=%u sprites=%u bgp=%02x\n",
                         int(state), pallet::read(ctx, pallet::Map), x, y,
                         pallet::read(ctx, pallet::Font), pallet::read(ctx, pallet::UpdateSprites),
                         ctx->io[0x47]);
            last = int(state);
            lastx = x;
            lasty = y;
        }
    }
    if (state == pallet::View::Overworld && !battle_composed)
        battle3d::remember_terrain(ctx);
    bool was_menu = menu_overlay;
    menu_overlay = can_compose_menu(ctx);
    menu_blurred = false;
    menu_regions = menu_overlay ? menu_layout::classify(ctx->wram + 0x3a0) : menu_layout::Layout{};
    menu_full = menu_overlay && menu_regions.kind == menu_layout::Kind::Full;
    dialogue_overlay = menu_overlay && pallet::bottom_dialogue(ctx);
    warp_overlay = !battle_composed && can_compose_warp(ctx);
    if (warp_overlay)
        warp_destination = pallet::read(ctx, pallet::Map);
    active = enabled &&
             ((preview_map >= 0 && presented_scene(ctx)) || state == pallet::View::Overworld ||
              state == pallet::View::Battle || state == pallet::View::Pokedex || battle_composed ||
              menu_overlay || warp_overlay || load_fade || pc_composed || can_compose_area(ctx)) &&
             width > 0 && height > 0;
    if (!battle::normal(ctx) && !battle_composed)
        battle3d::reset();
    if (!active || failed) {
        eye.reset();
        if (presentation::blend.running) {
            active = true;
            original_frame(ctx, width, height);
            ++active_frames;
        }
        return;
    }
    if (!program && !initialize(ctx)) {
        failed = true;
        std::fprintf(stderr, "[3D] Initialization failed; keeping the original 2D view.\n");
        if (presentation::blend.running)
            original_frame(ctx, width, height);
        return;
    }
    if (load_fade) {
        // A savestate has no guest fade. The explicit successful-load callback
        // starts a short presentation-only fade; normal clock wrap is irrelevant.
        uint32_t now = SDL_GetTicks();
        // A GPU upload/driver stall cannot skip the whole cosmetic reveal.
        // Guest-driven BGP fades above remain entirely frame-exact; this clock
        // is used only for a savestate, which has no guest fade of its own.
        if (!menu_open && window_focused)
            load_elapsed += std::min(25.f, float(uint32_t(now - load_started)));
        load_started = now;
        float elapsed = load_elapsed;
        bool entering = false;
        if (!load_incoming && elapsed >= 90) {
            load_incoming = true;
            load_started = SDL_GetTicks();
            load_elapsed = elapsed = 0;
            camera_ready = false;
            eye.reset();
            entering = true;
        }
        bool live = state == pallet::View::Overworld || dialogue_overlay;
        if (!live) {
            // Input can open a full-screen menu during these 180 ms. Its map
            // buffers may already be repurposed; never feed them to world().
            load_fade = false;
            draw_world_frame(width, height);
            scene_filter::capture(width, height);
            scene_filter::draw(width, height);
            lcd_overlay::framed(gb_get_framebuffer(ctx), float(width), float(height));
            ++active_frames;
            return;
        } else if (!load_incoming) {
            draw_world_frame(width, height, {1 - elapsed / 90, 0});
            ++active_frames;
            return;
        } else if (elapsed < 90) {
            world(ctx, width, height, {elapsed / 90, 0});
            // A cold tileset/mesh upload must not consume the reveal itself.
            // Start its clock after the destination has been drawn in black.
            if (entering)
                load_started = SDL_GetTicks();
            ++active_frames;
            return;
        } else
            load_fade = false;
    }
    if (battle_composed) {
        eye.reset();
        scene_filter::invalidate();
        if (battle_arena)
            battle3d::draw(ctx, width, height, menu_open, !battle_fighters);
        else {
            auto tone = battle_dark ? fade::Tone{0, 0} : fade::tone(ctx->io[0x47]);
            float progress =
                battle_phase.phase == battle_transition::Phase::Wipe ? battle_phase.wipe : 0;
            draw_world_frame(width, height, tone, false, false, progress);
            if (progress > 0 && scene_filter::capture(width, height)) {
                auto center = battle_zoom_center();
                scene_filter::draw(width, height, 1, 0, progress * .16f, (center[0] + 1) * .5f,
                                   (center[1] + 1) * .5f);
            }
        }
        ++active_frames;
        return;
    }
    if (warp_overlay) {
        auto tone = fade::tone(ctx->io[0x47]);
        if (!(ctx->io[0x40] & 0x80))
            tone = {0, ctx->io[0x47] == 0 ? 1.f : 0.f};
        bool hidden =
            !pallet::read(ctx, pallet::Sprite1) || pallet::read(ctx, pallet::Sprite1 + 2) == 255;
        draw_world_frame(width, height, tone, hidden, false);
        ++active_frames;
        return;
    }
    if (pc_composed) {
        pc3d::draw(ctx, width, height, menu_open);
        ++active_frames;
        return;
    }
    if (can_compose_area(ctx)) {
        if (dex_area3d::draw(ctx, width, height, menu_open)) {
            ++active_frames;
            return;
        }
        // Unsupported AREA data keeps the existing full-LCD compositor, even
        // on a cold load where no world frame has ever been resident.
        if (!menu_overlay) {
            original_frame(ctx, width, height);
            ++active_frames;
            return;
        }
    }
    if (state == pallet::View::Pokedex && preview_map < 0) {
        if (dex3d::draw(ctx, width, height, menu_open)) {
            ++active_frames;
            return;
        }
        if (!menu_overlay) {
            glViewport(0, 0, width, height);
            glClearColor(.035f, .065f, .075f, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            if (!menu_open)
                lcd_overlay::framed(gb_get_framebuffer(ctx), float(width), float(height));
            ++active_frames;
            return;
        }
    }
    if (menu_overlay) {
        // Only an initial recognized dialogue needs live preparation. Once a
        // scene is resident, party/PC buffers and menu VRAM are never consulted.
        bool prepare = world_frame.map < 0 || (dialogue_overlay && can_compose_dialogue(ctx));
        if (prepare) {
            world(ctx, width, height);
            scene_filter::invalidate();
        } else
            draw_world_frame(width, height);
        if (!was_menu)
            scene_filter::invalidate();
        if (menu_full) {
            if (!scene_filter::valid || scene_filter::width != width ||
                scene_filter::height != height)
                scene_filter::capture(width, height);
            menu_blurred = scene_filter::draw(width, height);
            if (!menu_blurred)
                draw_world_frame(width, height, {.42f, 0});
            if (!menu_open)
                lcd_overlay::framed(gb_get_framebuffer(ctx), float(width), float(height));
        } else if (!menu_open)
            lcd_overlay::regions(gb_get_framebuffer(ctx), menu_regions, float(width),
                                 float(height));
        ++active_frames;
        return;
    }
    scene_filter::invalidate();
    if (state == pallet::View::Battle && preview_map < 0) {
        eye.reset();
        battle3d::draw(ctx, width, height, menu_open);
        ++active_frames;
        return;
    }
    world(ctx, width, height);
    if (!menu_open && !dialogue_overlay)
        hud(ctx);
    if (dialogue_overlay && !menu_open) {
        lcd_overlay::draw(gb_get_framebuffer(ctx), lcd_overlay::Bottom, float(width), float(height),
                          1, true);
    }
    ++active_frames;
}

bool pallet3d_event(const SDL_Event *event, bool menu_open) {
    if (event->type == SDL_WINDOWEVENT) {
        if (event->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            window_focused = false;
            presentation::blend.paused = true;
        }
        if (event->window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
            window_focused = true;
        if (load_fade && (event->window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
                          event->window.event == SDL_WINDOWEVENT_FOCUS_GAINED))
            load_started = SDL_GetTicks();
    }
    pallet3d_poll_controls(input_context, menu_open);
    if (menu_open)
        return false;
    if (event->type == SDL_KEYDOWN && !event->key.repeat &&
        event->key.keysym.scancode == SDL_SCANCODE_F2) {
        enabled = !enabled;
        controls.reset();
        pallet3d_poll_controls(input_context, menu_open);
        std::fprintf(stderr, "[3D] %s\n", enabled ? "Enabled" : "Disabled");
        return true;
    }
    if (event->type == SDL_KEYDOWN && !event->key.repeat &&
        event->key.keysym.scancode == SDL_SCANCODE_F3) {
        if (can_compose_area(input_context))
            return true;
        first_person = !first_person;
        eye.reset();
        controls.reset();
        pallet3d_poll_controls(input_context, menu_open);
        return true;
    }
    if (relative_allowed && (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP)) {
        auto key = event->key.keysym.scancode;
        bool down = event->type == SDL_KEYDOWN;
        if (key == SDL_SCANCODE_W) {
            controls.forward = down;
            return true;
        }
        int amount = key == SDL_SCANCODE_A   ? -1
                     : key == SDL_SCANCODE_D ? 1
                     : key == SDL_SCANCODE_S ? 2
                                             : 0;
        if (amount) {
            if (down && !event->key.repeat)
                controls.request_turn(pallet::read(input_context, 0xc109), amount,
                                      input_context->cycles);
            return true;
        }
    }
    if (!active || warp_overlay || menu_overlay || load_fade || battle_composed ||
        dex3d::presented || dex_area3d::presented || pc_composed)
        return false;
    if (input_context && pallet::view(input_context) == pallet::View::Battle)
        return false;
    if (first_person)
        return false;
    const auto *room = input_context ? presented_scene(input_context) : nullptr;
    if (room && room->interior) {
        // Every room may turn and zoom; small rooms are exactly the ones that need it.
        if (event->type == SDL_MOUSEWHEEL) {
            room_zoom = std::clamp(room_zoom + event->wheel.y * .12f, .75f, 1.8f);
            return true;
        }
        if (event->type == SDL_KEYDOWN) {
            auto key = event->key.keysym.scancode;
            if (key == SDL_SCANCODE_Q || key == SDL_SCANCODE_E) {
                room_yaw = std::clamp(room_yaw + (key == SDL_SCANCODE_Q ? -.08f : .08f), -.4f, .4f);
                return true;
            }
            if (key == SDL_SCANCODE_R) {
                room_yaw = RoomYaw;
                room_zoom = 1;
                return true;
            }
        }
    }
    if (event->type == SDL_MOUSEWHEEL) {
        zoom = std::clamp(zoom + event->wheel.y * .12f, .7f, 2.8f);
        return true;
    }
    if (event->type == SDL_KEYDOWN) {
        switch (event->key.keysym.scancode) {
        case SDL_SCANCODE_Q:
            yaw = std::max(-.75f, yaw - .08f);
            return true;
        case SDL_SCANCODE_E:
            yaw = std::min(.75f, yaw + .08f);
            return true;
        case SDL_SCANCODE_R:
            yaw = -.32f;
            zoom = 1.f;
            return true;
        default:
            break;
        }
    }
    return false;
}

void pallet3d_shutdown() {
    presentation::shutdown();
    presented_lcd = nullptr;
    battle_sequence = battle_arena = battle_fighters = battle_dark = battle_composed = false;
    battle3d::shutdown();
    dex3d::shutdown();
    dex_area3d::shutdown();
    pc3d::shutdown();
    lcd_overlay::shutdown();
    scene_filter::shutdown();
    menu_overlay = menu_full = menu_blurred = false;
    for (auto &entry : meshes)
        glDeleteBuffers(1, &entry.second.buffer);
    meshes.clear();
    visible_maps.clear();
    last_component = -1;
    mesh_builds = 0;
    preview_map = -1;
    if (buffer)
        glDeleteBuffers(1, &buffer);
    if (atlas)
        glDeleteTextures(1, &atlas);
    if (program)
        glDeleteProgram(program);
    buffer = atlas = program = 0;
    active = false;
    failed = false;
    active_frames = 0;
    captured = false;
    sprites_uploaded = false;
    atlas_tileset = -2;
    tile_animation_info = {};
    world_frame = {};
    warp_overlay = false;
    warp_destination = -1;
    load_fade = load_incoming = false;
    scenery.clear();
    vertices.clear();
    camera_ready = false;
    eye.reset();
    first_person = false;
    controls.reset();
    input_mask = 0xff;
    pallet3d_set_input_mask(0xff, false);
    input_context = nullptr;
    relative_allowed = false;
    window_focused = true;
    dialogue_overlay = false;
}

void pallet3d_capture(int width, int height) {
    // Optional development capture: the actual final GL surface, including HUD.
    const char *path = std::getenv("PALLET3D_CAPTURE");
    if (!path || captured || !active || active_frames < 30)
        return;
    std::vector<uint8_t> rgba(size_t(width) * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    FILE *f = std::fopen(path, "wb");
    if (!f)
        return;
    std::fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (int y = height - 1; y >= 0; y--)
        for (int x = 0; x < width; x++)
            std::fwrite(&rgba[(y * width + x) * 4], 1, 3, f);
    std::fclose(f);
    captured = true;
    std::fprintf(stderr, "[3D] Captured %s\n", path);
}

bool pallet3d_active() {
    return active &&
           ((program && !failed) || (presentation::blend.running && presentation::program));
}

void pallet3d_begin_frame(GBContext *ctx, bool menu_open, const uint32_t *framebuffer) {
    presented_lcd = framebuffer;
    presentation::begin(wants_scene(ctx), menu_open || !window_focused, SDL_GetTicks());
    pallet3d_poll_controls(ctx, menu_open);
}
void pallet3d_finish_frame(int width, int height, bool menu_open) {
    presentation::finish(width, height, menu_open || !window_focused);
}
PalletBlendInfo pallet3d_blend() {
    return {presentation::blend.running, presentation::blend.target, presentation::blend.paused,
            presentation::blend.progress()};
}

bool pallet3d_covers_frame(const GBContext *ctx) {
    return (presentation::blend.running && presentation::source.valid) || frozen_blend(ctx) ||
           (program && wants_scene(ctx));
}

Pallet3DStats pallet3d_stats() {
    size_t count = 0;
    for (const auto &entry : meshes)
        count += entry.second.count;
    return {meshes.size(), mesh_builds, count, count * sizeof(Vertex)};
}
void pallet3d_preview(int map_id) {
    preview_map = map_id;
    camera_ready = false;
}

bool pallet3d_firstperson() {
    return first_person;
}

void pallet3d_poll_controls(GBContext *ctx, bool menu_open) {
    input_context = ctx;
    bool blending = presentation::blend.running ||
                    (presentation::history.valid && wants_scene(ctx) != presentation::blend.target);
    relative_allowed = first_person && enabled && !failed && !blending && window_focused &&
                       !menu_open && preview_map < 0 && !can_compose_battle(ctx) &&
                       pallet::view(ctx) == pallet::View::Overworld && !can_compose_warp(ctx) &&
                       !can_compose_menu(ctx) && !can_compose_pc(ctx) && !can_compose_area(ctx) &&
                       !load_fade;
    int facing = relative_allowed ? pallet::read(ctx, 0xc109) : 0;
    bool idle = relative_allowed && !pallet::read(ctx, pallet::Walk) &&
                !pallet::read(ctx, 0xd527) && pallet::read(ctx, 0xcc4b) == 1;
    auto previous_mask = input_mask;
    input_mask = controls.update(relative_allowed, facing, idle, ctx ? ctx->cycles : 0);
    if (std::getenv("PALLET3D_INPUT_TRACE") && ctx && previous_mask != input_mask)
        std::fprintf(
            stderr,
            "[FPINPUT] cycle=%llu mask=%02x facing=%d idle=%d walk=%d moving=%d last=%d ready=%d\n",
            (unsigned long long)ctx->cycles, input_mask, facing, idle,
            pallet::read(ctx, pallet::Walk), pallet::read(ctx, 0xd527), pallet::read(ctx, 0xd528),
            pallet::read(ctx, 0xcc4b));
    pallet3d_set_input_mask(input_mask, relative_allowed);
}
uint8_t pallet3d_input_mask() {
    return input_mask;
}

void pallet3d_state_loaded(GBContext *ctx) {
    dex_area3d::reset();
    pc3d::reset();
    battle_sequence = battle_arena = battle_fighters = battle_dark = battle_composed = false;
    battle3d::reset();
    warp_overlay = false;
    warp_destination = -1;
    menu_overlay = false;
    scene_filter::invalidate();
    controls.reset();
    load_fade = enabled && program && !failed && ctx && world_frame.map >= 0 &&
                world_frame.map != pallet::read(ctx, pallet::Map) &&
                pallet::view(ctx) == pallet::View::Overworld;
    load_incoming = false;
    load_started = SDL_GetTicks();
    load_elapsed = 0;
    if (!load_fade) {
        camera_ready = false;
        eye.reset();
    }
    pallet3d_poll_controls(ctx, false);
}
bool pallet3d_load_fade() {
    return load_fade;
}

bool pallet3d_warp_overlay() {
    return warp_overlay && active;
}
PalletBattleTransitionInfo pallet3d_battle_transition() {
    return {battle_composed && active, battle_composed && active && battle_arena,
            int(battle_phase.phase), battle_phase.wipe};
}
PalletWorldFrameInfo pallet3d_world_frame() {
    uint64_t hash = 14695981039346656037ull;
    const auto *bytes = reinterpret_cast<const uint8_t *>(world_frame.matrix.data());
    for (size_t i = 0; i < sizeof(world_frame.matrix); i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    return {world_frame.map, hash};
}

PalletMenuInfo pallet3d_menu() {
    return {menu_overlay && active && !dex3d::presented && !pc3d::presented, menu_full,
            menu_blurred, int(menu_regions.count)};
}
PalletPCInfo pallet3d_pc() {
    return {pc3d::presented && active,
            pc_sample.mode != pc_state::Mode::None,
            pc3d::monitor_image,
            int(pc_sample.mode),
            pc_terminal.map,
            pc_terminal.x,
            pc_terminal.z,
            pc_motion.amount,
            pc3d::matrix,
            pc3d::screen};
}
PalletPCDetailsInfo pallet3d_pc_details() {
    bool drawn = pc3d::presented && active;
    return {drawn && pc3d::stored_items.valid,
            drawn && pc3d::dex_rating.valid,
            pc3d::stored_items.count,
            pc3d::stored_items.quantity,
            pc3d::dex_rating.seen,
            pc3d::dex_rating.caught,
            pc3d::counter_screen};
}
PalletHallInfo pallet3d_hall() {
    return {pc_hall3d::presented && active,
            pc_hall3d::team.index,
            pc_hall3d::team.selected,
            pc_hall3d::team.count,
            pc_hall3d::team.fingerprint,
            pc_hall3d::builds,
            pc_hall3d::uploads,
            pc_hall3d::camera_x};
}
PalletStorageInfo pallet3d_storage() {
    PalletStorageInfo info{};
    info.active = pc_boxes::presented && active;
    info.box = pc_boxes::data.active;
    info.selected = pc_boxes::selected.index;
    info.party = pc_boxes::selected.party;
    info.fingerprint = pc_boxes::data.fingerprint;
    info.rebuilds = pc_boxes::rebuilds;
    info.uploads = pc_boxes::uploads;
    info.cached = pc_boxes::portraits.resident();
    for (int i = 0; i < 12; i++)
        info.counts[i] = pc_boxes::data.boxes[i].count;
    if (info.box >= 0 && info.box < 12)
        for (int i = 0; i < 20; i++)
            info.species[i] = pc_boxes::data.boxes[info.box].mons[i].species;
    for (int i = 0; i < 6; i++)
        info.species[20 + i] = pc_boxes::data.party.mons[i].species;
    return info;
}
PalletDexInfo pallet3d_dex() {
    return {active && dex3d::presented,
            dex3d::presented && dex3d::picture.valid,
            dex3d::species,
            battle::fingerprint(dex3d::image),
            dex3d::uploads,
            dex3d::list_mode,
            int(dex3d::selection.registration),
            dex3d::selection.number,
            dex3d::selection.seen,
            dex3d::selection.caught,
            dex3d::cache.resident(),
            dex3d::cache.decodes,
            dex3d::selection.row};
}

bool pallet3d_dialogue_overlay() {
    return dialogue_overlay && active;
}
PalletOverlayInfo pallet3d_overlay_stats() {
    return {lcd_overlay::uploads, lcd_overlay::bytes};
}
PalletCameraInfo pallet3d_camera() {
    return {eye.x, eye.y, eye.z, eye.yaw, actors_drawn, hero_drawn};
}
PalletBattleInfo pallet3d_battle() {
    return {
        pallet3d_active() && input_context &&
            (battle_composed ? battle_arena : pallet::view(input_context) == pallet::View::Battle),
        battle3d::full_overlay || battle3d::overlay_alpha > 0,
        battle3d::terrain,
        battle3d::portraits[0].species,
        battle3d::portraits[1].species,
        battle3d::portraits[0].alpha,
        battle3d::portraits[1].alpha,
        battle3d::portraits[0].fingerprint,
        battle3d::portraits[1].fingerprint,
        int(battle3d::effect_kind),
        battle3d::capture_effect,
        battle3d::effect_time,
        battle3d::effect_actor,
        battle3d::overlay_alpha,
        battle3d::trainer_class,
        battle3d::portraits[0].hp,
        battle3d::portraits[1].hp,
        battle3d::portraits[0].damage,
        battle3d::portraits[1].damage};
}

PalletAreaInfo pallet3d_area() {
    return {active && dex_area3d::presented,
            dex_area3d::ready,
            dex_area3d::blink,
            dex_area3d::species,
            dex_area3d::nests.locations.size(),
            dex_area3d::nests.coordinates.size(),
            dex_area3d::markers.size(),
            dex_area3d::builds,
            dex_area3d::count,
            dex_area3d::uploads};
}

PalletTileAnimationInfo pallet3d_tile_animation() {
    return tile_animation_info;
}
