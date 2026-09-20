#pragma once

#include "gbrt.h"
#include <SDL.h>

// Presentation only: these functions never write to the emulated machine.
void pallet3d_draw(GBContext* ctx, int width, int height, bool menu_open);
bool pallet3d_event(const SDL_Event* event, bool menu_open);
void pallet3d_shutdown();
void pallet3d_capture(int width, int height);

// Read-only diagnostic of the last presented frame.
bool pallet3d_active();
// True only when an initialized 3D renderer will cover the original framebuffer.
bool pallet3d_covers_frame(const GBContext* ctx);

struct Pallet3DStats { size_t resident_maps, mesh_builds, vertices, bytes; };
Pallet3DStats pallet3d_stats();
// Diagnostic preview uses the same geometry pipeline without changing game RAM.
// Pass -1 to return to the live game. No actors are invented for preview maps.
void pallet3d_preview(int map_id);

bool pallet3d_firstperson();
void pallet3d_poll_controls(GBContext* ctx,bool menu_open);
uint8_t pallet3d_input_mask();
bool pallet3d_dialogue_overlay();
struct PalletMenuInfo {bool active,full,blurred;int regions;};
PalletMenuInfo pallet3d_menu();
bool pallet3d_warp_overlay();
// Called only after a successful SDL savestate load (also used by private QA).
void pallet3d_state_loaded(GBContext* ctx);
bool pallet3d_load_fade();
struct PalletWorldFrameInfo {int map;uint64_t camera;};
PalletWorldFrameInfo pallet3d_world_frame();
struct PalletOverlayInfo {size_t uploads,bytes;};
PalletOverlayInfo pallet3d_overlay_stats();
struct PalletCameraInfo {float x,y,z,yaw;int actors;bool player_drawn;};
PalletCameraInfo pallet3d_camera();
struct PalletBattleInfo {
    bool active,full_overlay;int terrain,player_species,enemy_species;
    float player_alpha,enemy_alpha;
    uint64_t player_image,enemy_image; // Fingerprints of the uploaded portraits.
    int effect;bool capture;float effect_time;int effect_actor;float overlay_alpha;
    int trainer_class;
    float player_hp,enemy_hp,player_damage,enemy_damage;
};
PalletBattleInfo pallet3d_battle();
