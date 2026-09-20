#pragma once

#include "gbrt.h"
#include <SDL.h>
#include <array>

// Register before initializing the runtime SDL frontend.
bool pallet3d_register();

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
// Diagnostic preview uses the same geometry pipeline without changing game RAM.
// Pass -1 to return to the live game. No actors are invented for preview maps.
void pallet3d_preview(int map_id);

bool pallet3d_firstperson();
void pallet3d_poll_controls(GBContext *ctx, bool menu_open);
uint8_t pallet3d_input_mask();
bool pallet3d_dialogue_overlay();
struct PalletMenuInfo {
    bool active, full, blurred;
    int regions;
};
PalletMenuInfo pallet3d_menu();
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
