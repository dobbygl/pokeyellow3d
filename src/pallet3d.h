#pragma once

#include "gbrt.h"
#include "daylight.h"
#include "ui_preferences.h"
#include <SDL.h>
#include <array>

// The same callback table is used by the playable frontend and QA adapters.
struct GBPresentationHooks;
GBPresentationHooks pallet3d_presentation_hooks();

// Register before initializing the runtime SDL frontend.
bool pallet3d_register();
bool pallet3d_menu_style(ui_preferences::Style style, bool persist = false);
ui_preferences::Style pallet3d_menu_style();
bool pallet3d_window_focused();
// Independent presentation preference; changing it invalidates cached art meshes.
bool pallet3d_artistic(bool enabled, bool persist = false);
bool pallet3d_artistic();

// Presentation only: these functions never write to the emulated machine.
void pallet3d_draw(GBContext *ctx, int width, int height, bool menu_open);
bool pallet3d_event(const SDL_Event *event, bool menu_open);
void pallet3d_shutdown();
void pallet3d_capture(int width, int height);
// Frame boundaries surround both the game scene and its ImGui composition.
void pallet3d_begin_frame(GBContext *ctx, bool menu_open, const uint32_t *framebuffer = nullptr);
void pallet3d_finish_frame(int width, int height, bool menu_open);
struct PalletBlendInfo {
    bool active, target_3d, paused;
    float progress;
};
PalletBlendInfo pallet3d_blend();

// Read-only diagnostic of the last presented frame.
bool pallet3d_active();
// True only when an initialized 3D renderer will cover the original framebuffer.
bool pallet3d_covers_frame(const GBContext *ctx);

struct Pallet3DStats {
    size_t resident_maps, mesh_builds, vertices, bytes;
};
Pallet3DStats pallet3d_stats();
struct PalletShadowInfo {
    bool ready, active, artistic_scene, player_caster;
    size_t passes;
    unsigned texture;
};
// Read-only diagnostics for the actual RGBA depth target and pass submission.
PalletShadowInfo pallet3d_shadows();
// Diagnostic preview uses the same geometry pipeline without changing game RAM.
// Pass -1 to return to the live game. No actors are invented for preview maps.
void pallet3d_preview(int map_id);
void pallet3d_preview(int map_id, bool first_person);
bool pallet3d_preview_firstperson();

bool pallet3d_firstperson();
void pallet3d_poll_controls(GBContext *ctx, bool menu_open);
uint8_t pallet3d_input_mask();
bool pallet3d_dialogue_overlay();
struct PalletMenuInfo {
    bool active, full, blurred;
    int regions;
};
PalletMenuInfo pallet3d_menu();
struct PalletFullMenuInfo {
    bool active = false, fallback = false;
    int context = 0, kind = 0, regions = 0;
    int current = 0, scroll = 0, selected = 0, cursor_x = -1, cursor_y = -1;
};
PalletFullMenuInfo pallet3d_full_menu();
struct PalletPokemonMenuInfo {
    bool active = false, fallback = false;
    int kind = 0, selected = 0, species = 0;
    size_t bars = 0;
};
PalletPokemonMenuInfo pallet3d_pokemon_menu();

struct PalletDexInfo {
    bool active, verified;
    int species;
    uint64_t image;
    size_t uploads;
    bool list;
    int registration, number, seen, caught;
    size_t cached, decodes;
    int row;
};
PalletDexInfo pallet3d_dex();
struct PalletDexMenuInfo {
    bool active = false, fallback = true;
    std::array<int, 4> left{}, top{}, scale{};
};
PalletDexMenuInfo pallet3d_dex_menu();
struct PalletAreaInfo {
    bool active, ready, blink;
    int species;
    size_t maps, locations, markers, builds, vertices, uploads;
};
PalletAreaInfo pallet3d_area();
struct PalletPCInfo {
    bool active, open, monitor;
    int mode, map, x, z;
    float progress;
    std::array<float, 16> matrix;
    std::array<float, 8> screen;
};
PalletPCInfo pallet3d_pc();
struct PalletPCDetailsInfo {
    bool items, oak;
    int stored, quantity, seen, caught;
    std::array<float, 8> counter_screen;
};
PalletPCDetailsInfo pallet3d_pc_details();
struct PalletStorageInfo {
    bool active, party;
    int box, selected;
    uint64_t fingerprint;
    size_t rebuilds, uploads, cached;
    std::array<int, 12> counts;
    std::array<int, 26> species;
};
PalletStorageInfo pallet3d_storage();
bool pallet3d_warp_overlay();
struct PalletBattleTransitionInfo {
    bool active, arena;
    int phase;
    float wipe;
};
PalletBattleTransitionInfo pallet3d_battle_transition();
// Called only after a successful SDL savestate load (also used by private QA).
void pallet3d_state_loaded(GBContext *ctx);
bool pallet3d_load_fade();
struct PalletWorldFrameInfo {
    int map;
    uint64_t camera;
};
PalletWorldFrameInfo pallet3d_world_frame();
struct PalletOverlayInfo {
    size_t uploads, bytes;
};
PalletOverlayInfo pallet3d_overlay_stats();
struct PalletCameraInfo {
    float x, y, z, yaw;
    int actors;
    bool player_drawn;
};
PalletCameraInfo pallet3d_camera();
struct PalletBattleInfo {
    bool active, full_overlay;
    int terrain, player_species, enemy_species;
    float player_alpha, enemy_alpha;
    uint64_t player_image, enemy_image; // Fingerprints of the uploaded portraits.
    int effect;
    bool capture;
    float effect_time;
    int effect_actor;
    float overlay_alpha;
    int trainer_class;
    float player_hp, enemy_hp, player_damage, enemy_damage;
    bool integrated_menu;
    int menu_kind, menu_panels;
};
PalletBattleInfo pallet3d_battle();

struct PalletHallInfo {
    bool active;
    int team, selected, count;
    uint64_t fingerprint;
    size_t builds, uploads;
    float camera_x;
};
PalletHallInfo pallet3d_hall();

// Read-only diagnostics for original tileset animation and private atlas QA.
struct PalletTileAnimationInfo {
    int water_shift = -1, flower_frame = -1;
    unsigned texture = 0;
    size_t uploads = 0;
};
PalletTileAnimationInfo pallet3d_tile_animation();
// Shared ROM atlas handle for independent draw-command/pixel QA.
unsigned pallet3d_font_texture();

struct PalletWorldAnimationInfo {
    bool enabled;
    float wind;
    size_t particles;
    uint32_t emitted;
};
PalletWorldAnimationInfo pallet3d_world_animation();
// Presentation preference and replay comparison; never changes engine state.
void pallet3d_world_effects(bool enabled);

// Preferences are separate from cartridge data. A private path also permits
// process-restart QA without touching the player's preferences.
void pallet3d_load_preferences(const char *path);
bool pallet3d_daylight(daynight::Settings settings, bool persist = false);
daynight::Settings pallet3d_daylight_settings();
daynight::Light pallet3d_daylight_frame();
void pallet3d_settings_ui(bool menu_open);
