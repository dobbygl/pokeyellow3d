// Local integration check using the user's ROM + a copied original savestate.
// No ROM or savestate is committed. Playback advances only the private copy.
// No mode invokes battery-save callbacks.
extern "C" {
#include "pokeyellow.h"
}
#include "platform_sdl.h"
#include "pallet3d.h"
#include "pallet_state.h"
#include "assets_manifest_pokeyellow.h"
#include "mon_pic_cache.h"
#include <SDL_opengles2.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
extern "C" uint8_t pokeyellow__rom_data[];
#include "world_journey.h"
#include "world_extended.h"
#include "qa_presentation_clock.h"

static void capture_surface(const char *path) {
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int w = viewport[2], h = viewport[3];
    std::vector<uint8_t> rgba(size_t(w) * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    FILE *f = std::fopen(path, "wb");
    if (!f)
        std::exit(19);
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; y--)
        for (int x = 0; x < w; x++)
            std::fwrite(&rgba[(y * w + x) * 4], 1, 3, f);
    std::fclose(f);
    qa_clock::capture_state(path);
}

#include "firstperson.h"
#include "firstperson_integration.h"
#include "menu_integration.h"
#include "interior_integration.h"
#include "battle_integration.h"
#include "battle_swap.h"
#include "battle_trainer.h"
#include "battle_effects.h"
#include "town_integration.h"
#include "interior_transitions.h"
#include "ui_transitions.h"
#include "battle_transition_trace.h"
#include "presentation_integration.h"
#include "special_transitions.h"
#include "dex_integration.h"
#include "pc_integration.h"
#include "pc_details_integration.h"
#include "pc_hall_integration.h"
#include "tile_animation_integration.h"
#include "title_integration.h"
#include "boot_integration.h"
#include "world_animation_integration.h"
#include "daylight_integration.h"

int main(int argc, char **argv) {
    SDL_SetMainReady();
    if (argc < 3) {
        std::fprintf(stderr, "Usage: pallet_render_smoke ROM SAVESTATE [camera]\n");
        std::fprintf(
            stderr,
            "       pallet_render_smoke ROM boot [menu|new|continue] [MAX_FRAMES] [BATTERY]\n");
        return 1;
    }
    FILE *f = std::fopen(argv[1], "rb");
    if (!f)
        return 2;
    std::vector<uint8_t> rom(1048576);
    size_t count = std::fread(rom.data(), 1, rom.size(), f);
    std::fclose(f);
    if (count != 1048576)
        return 3;
    // Match gb_load_assets: only the manifest sections are resident at runtime.
    std::memset(pokeyellow__rom_data, 0xff, 1048576);
    for (auto entry : POKEYELLOW_ASSETS_MANIFEST)
        std::memcpy(pokeyellow__rom_data + entry.rom_offset, rom.data() + entry.rom_offset,
                    entry.size);
    GBConfig config = *pokeyellow_default_config();
    config.model = config.cartridge_supports_cgb ? GB_MODEL_CGB : GB_MODEL_DMG;
    config.cgb_compatibility_mode = false;
    GBContext *ctx = gb_context_create(&config);
    if (!ctx || !qa_clock::install() || !gb_platform_init(5))
        return 4;
    gb_platform_register_context(ctx);
    gb_platform_set_game_id(ctx, "pokeyellow");
    if (argc > 5 && (!std::strcmp(argv[2], "--title-audit") || !std::strcmp(argv[2], "boot"))) {
        title_qa::battery_path = argv[5];
        ctx->callbacks.load_battery_ram = title_qa::load_battery;
    }
    pokeyellow_init(ctx);
    if (!std::strcmp(argv[2], "boot")) {
        int result =
            boot_qa::run(ctx, argc > 3 ? argv[3] : "menu", argc > 4 ? std::atoi(argv[4]) : 12000);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 4 && !std::strcmp(argv[2], "--title-audit")) {
        int result = title_qa::audit(ctx, argv[3], std::atoi(argv[4]));
        gb_platform_shutdown();
        return result;
    }
    if (!gb_context_load_state_file(ctx, argv[2]))
        return 5;
    if (argc > 3 &&
        (std::strcmp(argv[3], "firstperson") == 0 || std::strcmp(argv[3], "fp-camera") == 0)) {
        SDL_Event event{};
        event.type = SDL_KEYDOWN;
        event.key.keysym.scancode = SDL_SCANCODE_F3;
        if (!pallet3d_event(&event, false) || !pallet3d_firstperson())
            return 27;
    }
    std::fprintf(stderr, "[SMOKE] view=%d map=%u xy=%u,%u\n", int(pallet::view(ctx)),
                 pallet::read(ctx, pallet::Map), pallet::read(ctx, pallet::X),
                 pallet::read(ctx, pallet::Y));
    std::fprintf(
        stderr, "[SMOKE] party=%u hp=%u level=%u script=%u/%u font=%u\n", pallet::read(ctx, 0xd162),
        pallet::read(ctx, 0xd16b) * 256 + pallet::read(ctx, 0xd16c), pallet::read(ctx, 0xd18b),
        pallet::read(ctx, 0xd5f0), pallet::read(ctx, 0xd5ef), pallet::read(ctx, pallet::Font));
    if (argc > 3 && (!std::strcmp(argv[3], "daylight") || !std::strcmp(argv[3], "daylight-fp"))) {
        int result = daylight_qa::captures(ctx, !std::strcmp(argv[3], "daylight-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 4 && !std::strcmp(argv[3], "daylight-reload")) {
        int result = daylight_qa::reload(ctx, argv[4]);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 4 &&
        (!std::strcmp(argv[3], "daylight-replay") || !std::strcmp(argv[3], "daylight-replay-fp"))) {
        int result =
            daylight_qa::replay(ctx, std::strstr(argv[3], "-fp") != nullptr, std::atoi(argv[4]));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && (!std::strcmp(argv[3], "world-animation-grass") ||
                     !std::strcmp(argv[3], "world-animation-grass-fp") ||
                     !std::strcmp(argv[3], "world-animation-surf") ||
                     !std::strcmp(argv[3], "world-animation-surf-fp"))) {
        int result = world_animation_qa::effects(ctx, std::strstr(argv[3], "-fp") != nullptr,
                                                 std::strstr(argv[3], "-surf") != nullptr);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && (!std::strcmp(argv[3], "world-animation-npc") ||
                     !std::strcmp(argv[3], "world-animation-npc-fp"))) {
        int result = world_animation_qa::npc(ctx, !std::strcmp(argv[3], "world-animation-npc-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 &&
        (!std::strcmp(argv[3], "tile-animation") || !std::strcmp(argv[3], "tile-animation-fp"))) {
        int result = tile_animation_qa::run(ctx, !std::strcmp(argv[3], "tile-animation-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 4 && (!std::strcmp(argv[3], "travel") || !std::strcmp(argv[3], "travel-fp"))) {
        int result =
            special_transition_qa::travel(ctx, argv[4], !std::strcmp(argv[3], "travel-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 4 &&
        (!std::strcmp(argv[3], "load-pause") || !std::strcmp(argv[3], "load-pause-fp"))) {
        int result =
            special_transition_qa::load_pause(ctx, argv[4], !std::strcmp(argv[3], "load-pause-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 &&
        (!std::strcmp(argv[3], "stale-battle") || !std::strcmp(argv[3], "stale-battle-fp"))) {
        int result =
            special_transition_qa::stale_battle(ctx, !std::strcmp(argv[3], "stale-battle-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 4 && (!std::strcmp(argv[3], "transport") || !std::strcmp(argv[3], "transport-fp"))) {
        int result = special_transition_qa::transport(ctx, !std::strcmp(argv[4], "bike"),
                                                      !std::strcmp(argv[3], "transport-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && (!std::strcmp(argv[3], "crossfade") || !std::strcmp(argv[3], "crossfade-fp"))) {
        int result = presentation_qa::run(ctx, !std::strcmp(argv[3], "crossfade-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && !std::strcmp(argv[3], "dex-portraits")) {
        int result = dex_qa::portraits(ctx);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && !std::strcmp(argv[3], "dex-data")) {
        int result = dex_qa::portraits(ctx, true);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && (!std::strcmp(argv[3], "dex-list") || !std::strcmp(argv[3], "dex-list-fp"))) {
        int result = dex_qa::lists(ctx, !std::strcmp(argv[3], "dex-list-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && (!std::strcmp(argv[3], "dex") || !std::strcmp(argv[3], "dex-fp"))) {
        int result = dex_qa::areas(ctx, !std::strcmp(argv[3], "dex-fp"), true);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && !std::strcmp(argv[3], "dex-area-screen")) {
        int result = dex_qa::area_screen(ctx);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && !std::strcmp(argv[3], "dex-area-oracle")) {
        int result = dex_qa::areas(ctx);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 &&
        (!std::strcmp(argv[3], "dex-screen") || !std::strcmp(argv[3], "dex-screen-fp"))) {
        int result = dex_qa::screen(ctx, !std::strcmp(argv[3], "dex-screen-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 &&
        (!std::strcmp(argv[3], "pc-details") || !std::strcmp(argv[3], "pc-details-fp"))) {
        int result = pc_details_qa::items_and_oak(ctx, !std::strcmp(argv[3], "pc-details-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && (!std::strcmp(argv[3], "pc") || !std::strcmp(argv[3], "pc-fp"))) {
        int result = pc_hall_qa::journey(ctx, !std::strcmp(argv[3], "pc-fp"), true);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && (!std::strcmp(argv[3], "pc-hall") || !std::strcmp(argv[3], "pc-hall-fp"))) {
        int result = pc_hall_qa::journey(ctx, !std::strcmp(argv[3], "pc-hall-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && (!std::strcmp(argv[3], "pc-focus") || !std::strcmp(argv[3], "pc-focus-fp"))) {
        int result = pc_qa::focus(ctx, !std::strcmp(argv[3], "pc-focus-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && !std::strcmp(argv[3], "capture-pidgey")) {
        int result = battle_capture(ctx, 16);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 &&
        (!std::strcmp(argv[3], "pc-storage") || !std::strcmp(argv[3], "pc-storage-fp"))) {
        int result = pc_qa::storage(ctx, !std::strcmp(argv[3], "pc-storage-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && (!std::strcmp(argv[3], "pc-storage-stress") ||
                     !std::strcmp(argv[3], "pc-storage-stress-fp"))) {
        int result = pc_qa::storage_stress(ctx, !std::strcmp(argv[3], "pc-storage-stress-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 &&
        (!std::strcmp(argv[3], "crossfade-warp") || !std::strcmp(argv[3], "crossfade-warp-fp"))) {
        int result = presentation_qa::warp(ctx, !std::strcmp(argv[3], "crossfade-warp-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && std::strcmp(argv[3], "dialogue") == 0) {
        int result = dialogue_journey(ctx);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && (!std::strcmp(argv[3], "menus") || !std::strcmp(argv[3], "menus-fp"))) {
        int result = menus_journey(ctx, !std::strcmp(argv[3], "menus-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 &&
        (!std::strcmp(argv[3], "menus-center") || !std::strcmp(argv[3], "menus-center-fp"))) {
        int result = menus_center(ctx, !std::strcmp(argv[3], "menus-center-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 &&
        (!std::strcmp(argv[3], "menus-shop") || !std::strcmp(argv[3], "menus-shop-fp"))) {
        int result = menus_shop(ctx, !std::strcmp(argv[3], "menus-shop-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 &&
        (!std::strcmp(argv[3], "menus-name") || !std::strcmp(argv[3], "menus-name-fp"))) {
        int result = menus_name(ctx, !std::strcmp(argv[3], "menus-name-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 &&
        (!std::strcmp(argv[3], "transitions") || !std::strcmp(argv[3], "transitions-fp"))) {
        int result = transition_qa::run(ctx, !std::strcmp(argv[3], "transitions-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 4 && (!std::strcmp(argv[3], "transition-reload") ||
                     !std::strcmp(argv[3], "transition-reload-fp"))) {
        int result =
            transition_qa::reload(ctx, argv[4], !std::strcmp(argv[3], "transition-reload-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && !std::strcmp(argv[3], "transitions-white")) {
        int result = transition_qa::run(ctx, false, true);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && std::strcmp(argv[3], "ui-inspect") == 0) {
        std::printf("map=%d bgp=%02x lcdc=%02x font=%d sprites=%d bank=%d pc=%04x sp=%04x\n",
                    pallet::read(ctx, pallet::Map), ctx->io[0x47], ctx->io[0x40],
                    pallet::read(ctx, pallet::Font), pallet::read(ctx, pallet::UpdateSprites),
                    ctx->hram[0x38], ctx->pc, ctx->sp);
        for (int y = 0; y < 18; y++) {
            std::printf("%02d:", y);
            for (int x = 0; x < 20; x++)
                std::printf(" %02x", battle::tile(ctx, x, y));
            std::puts("");
        }
        gb_platform_shutdown();
        return 0;
    }
    if (argc > 3 &&
        (std::strcmp(argv[3], "interior") == 0 || std::strcmp(argv[3], "interior-fp") == 0)) {
        int result = interior_journey(ctx, std::strcmp(argv[3], "interior-fp") == 0);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && std::strcmp(argv[3], "battle-probe") == 0) {
        int result = battle_probe(ctx);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 &&
        (!std::strcmp(argv[3], "battle-timing") || !std::strcmp(argv[3], "trainer-timing") ||
         !std::strcmp(argv[3], "battle-timing-fp") || !std::strcmp(argv[3], "trainer-timing-fp"))) {
        int result = battle_transition_trace::run(ctx, !std::strncmp(argv[3], "trainer-", 8),
                                                  std::strstr(argv[3], "-fp"));
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && std::strcmp(argv[3], "battle-menus") == 0) {
        int result = battle_menus(ctx);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && std::strcmp(argv[3], "battle-capture") == 0) {
        int result = battle_capture(ctx);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && std::strcmp(argv[3], "battle-swap") == 0) {
        int result = battle_swap(ctx);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && std::strcmp(argv[3], "trainer-prepare") == 0) {
        int result = trainer_prepare(ctx);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && std::strcmp(argv[3], "battle-trainer") == 0) {
        int result = battle_trainer(ctx);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && std::strcmp(argv[3], "battle-effects") == 0) {
        int result = battle_effects(ctx, argv[2]);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && std::strcmp(argv[3], "battle3d") == 0) {
        if (battle_capture(ctx))
            return 40;
        if (!gb_context_load_state_file(ctx, "logs/capture-complete.state"))
            return 5;
        if (battle_swap(ctx))
            return 40;
        if (!gb_context_load_state_file(ctx, "logs/swap-return.state"))
            return 5;
        if (trainer_prepare(ctx))
            return 40;
        if (!gb_context_load_state_file(ctx, "logs/route22-trainer.state"))
            return 5;
        if (battle_trainer(ctx))
            return 40;
        if (battle_effects(ctx, "logs/capture-start.state"))
            return 40;
        gb_platform_shutdown();
        std::puts("PASS: battle3d capture, swaps, trainer, fainting, four effects and exact "
                  "original fallback");
        return 0;
    }
    if (argc > 3 && std::strcmp(argv[3], "town") == 0) {
        int result = town_journey(ctx);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && std::strcmp(argv[3], "interior-transitions") == 0) {
        int result = interior_transitions(ctx);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && std::strcmp(argv[3], "battle-inspect") == 0) {
        std::fprintf(stderr,
                     "[BATTLE-INSPECT] LCDC=%02x SCX=%d SCY=%d BGP=%02x ready=%d animation=%d "
                     "SP=%04x bank=%d\n",
                     ctx->io[0x40], ctx->io[0x43], ctx->io[0x42], ctx->io[0x47], battle::ready(ctx),
                     battle::animation_running(ctx), ctx->sp, ctx->rom_bank);
        for (int y = 0; y < 7; y++) {
            std::fprintf(stderr, "[RECT] y=%d wram=", y);
            for (int x = 0; x < 7; x++)
                std::fprintf(stderr, "%02x ", battle::tile(ctx, 12 + x, y));
            std::fprintf(stderr, "vram9800=");
            for (int x = 0; x < 7; x++)
                std::fprintf(stderr, "%02x ", ctx->vram[0x1800 + y * 32 + 12 + x]);
            std::fprintf(stderr, "vram9c00=");
            for (int x = 0; x < 7; x++)
                std::fprintf(stderr, "%02x ", ctx->vram[0x1c00 + y * 32 + 12 + x]);
            std::fprintf(stderr, "\n");
        }
        std::fprintf(stderr, "[STACK]");
        for (int a = ctx->sp; a < 0xe000; a++)
            std::fprintf(stderr, " %02x", pallet::read(ctx, a));
        std::fprintf(stderr, "\n");
        gb_platform_shutdown();
        return 0;
    }
    if (argc > 3 && (std::strcmp(argv[3], "fp-benchmark") == 0 ||
                     std::strcmp(argv[3], "ortho-benchmark") == 0)) {
        int result = firstperson_benchmark(ctx, std::strcmp(argv[3], "fp-benchmark") == 0);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && std::strcmp(argv[3], "fp-views") == 0) {
        int result = firstperson_views(ctx);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && std::strcmp(argv[3], "fp-house") == 0) {
        int result = firstperson_house(ctx);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && std::strcmp(argv[3], "fp-controls") == 0) {
        int result = firstperson_controls(ctx);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 5 && std::strcmp(argv[3], "fp-replay") == 0) {
        int result = firstperson_replay(ctx, argv[4], argv[5]);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 5 && std::strcmp(argv[3], "prepare") == 0) {
        int result = extended_prepare(ctx, argv[4], argv[5]);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 6 && std::strcmp(argv[3], "warp") == 0) {
        // QA fixture setup only: request the engine's scripted warp. The game
        // loads the header, blocks, sprites and entry coordinates itself.
        ctx->hram[0x0b] = uint8_t(std::atoi(argv[4]));
        ctx->wram[0x142e] = uint8_t(std::atoi(argv[5]));
        ctx->wram[0x172c] |= 8;
        gb_platform_set_input_script(nullptr);
        for (int frame = 0; frame < 350; frame++) {
            gb_reset_frame(ctx);
            ctx->stopped = 0;
            unsigned slices = 0;
            while (!ctx->frame_done) {
                gb_run_cycles(ctx, 70224);
                if (!gb_platform_poll_events(ctx) || ++slices > 1000)
                    return 12;
            }
            std::vector<uint8_t> before(ctx->wram, ctx->wram + 8192);
            gb_platform_render_frame(gb_get_framebuffer(ctx));
            if (glGetError() != GL_NO_ERROR || std::memcmp(before.data(), ctx->wram, 8192))
                return 21;
        }
        std::fprintf(stderr, "[WARP] map=%u xy=%u,%u view=%d\n", pallet::read(ctx, pallet::Map),
                     pallet::read(ctx, pallet::X), pallet::read(ctx, pallet::Y),
                     int(pallet::view(ctx)));
        if (pallet::read(ctx, pallet::Map) != std::atoi(argv[4]))
            return 24;
        if (!gb_context_save_state_file(ctx, argv[6]))
            return 18;
        gb_platform_shutdown();
        return 0;
    }
    if (argc > 3 &&
        (std::strcmp(argv[3], "catalog") == 0 || std::strcmp(argv[3], "interior-catalog") == 0)) {
        bool interiors = !std::strcmp(argv[3], "interior-catalog");
        if (interiors)
            pallet::catalog->discover_warps(kanto::Rom(ctx->rom, ctx->rom_size));
        ReadOnlyMemory before{ctx};
        std::fprintf(stderr, "[CATALOG] GPU=%s\n", glGetString(GL_RENDERER));
        for (const auto &entry : pallet::catalog->maps) {
            if (interiors && !entry.second.interior)
                continue;
            pallet3d_preview(entry.first);
            for (int frame = 0; frame < 3; frame++) {
                gb_platform_render_frame(gb_get_framebuffer(ctx));
                if (glGetError() != GL_NO_ERROR || !pallet3d_active() || !before.unchanged(ctx))
                    return 21;
            }
            auto stats = pallet3d_stats();
            if (stats.resident_maps != 1 || !stats.vertices)
                return 22;
            if (const char *benchmark = std::getenv("CATALOG_BENCH_FRAMES")) {
                int count = std::max(1, std::atoi(benchmark));
                glFinish();
                auto start = std::chrono::steady_clock::now();
                for (int i = 0; i < count; i++) {
                    gb_platform_render_frame(gb_get_framebuffer(ctx));
                    glFinish();
                }
                double ms = std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - start)
                                .count() /
                            count;
                std::fprintf(stderr, "[PERF] map=%d frames=%d mean_ms=%.3f\n", entry.first, count,
                             ms);
            }
            if (const char *dir = std::getenv("CATALOG_CAPTURE_DIR")) {
                std::string path =
                    std::string(dir) + "/map-" + std::to_string(entry.first) + ".ppm";
                capture_surface(path.c_str());
            }
            std::fprintf(stderr, "[CATALOG] map=%d vertices=%zu bytes=%zu\n", entry.first,
                         stats.vertices, stats.bytes);
        }
        pallet3d_preview(-1);
        gb_platform_render_frame(gb_get_framebuffer(ctx));
        if (!before.unchanged(ctx))
            return 21;
        gb_platform_shutdown();
        std::puts(
            interiors
                ? "PASS: all 179 interior meshes, bounded eviction, OpenGL and read-only previews"
                : "PASS: all 38 meshes, bounded eviction, OpenGL and read-only previews");
        return 0;
    }
    if (argc > 3 &&
        (std::strcmp(argv[3], "journey") == 0 || std::strcmp(argv[3], "viridian") == 0 ||
         std::strcmp(argv[3], "firstperson") == 0)) {
        int result = world_journey(ctx, std::strcmp(argv[3], "viridian") == 0);
        gb_platform_shutdown();
        return result;
    }
    if (argc > 3 && std::strcmp(argv[3], "turn-probe") == 0) {
        QaWalk run{ctx};
        for (int duration = 1; duration <= 12; duration++) {
            if (!gb_context_load_state_file(ctx, argv[2]))
                return 5;
            run.input(nullptr);
            run.wait(30);
            int x = run.read(pallet::X), y = run.read(pallet::Y), facing = run.read(0xc109);
            run.press("U", duration);
            std::fprintf(stderr, "[TURN] duration=%d facing=%d -> %d delta=%d,%d\n", duration,
                         facing, run.read(0xc109), run.read(pallet::X) - x,
                         run.read(pallet::Y) - y);
        }
        gb_platform_shutdown();
        return 0;
    }
    if (argc > 3 && std::strcmp(argv[3], "inspect") == 0) {
        std::fprintf(stderr, "[MODE] bike/surf=%u standing=%02x facing=%u front=%02x\n",
                     pallet::read(ctx, 0xd6ff), ctx->hram[0x7b], pallet::read(ctx, 0xc109),
                     pallet::read(ctx, 0xcfc5));
        if (auto *scene = pallet::scene(pallet::read(ctx, pallet::Map))) {
            int n = 0, bw = scene->width / 2;
            for (int z = 0; z < scene->height / 2; z++)
                for (int x = 0; x < bw; x++) {
                    int live = pallet::read(ctx, 0xc6e8 + (z + 3) * (bw + 6) + x + 3),
                        base = scene->block_data[z * bw + x];
                    if (live != base && n++ < 15)
                        std::fprintf(stderr, "[BLOCK] xy=%d,%d rom=%02x live=%02x\n", x, z, base,
                                     live);
                }
            std::fprintf(stderr, "[BLOCK] differing=%d ptr=%02x%02x\n", n,
                         pallet::read(ctx, 0xd36a), pallet::read(ctx, 0xd369));
        }
        gb_platform_shutdown();
        return 0;
    }
    if (argc > 4 && std::strcmp(argv[3], "reload") == 0) {
        auto render = [&]() {
            for (int i = 0; i < 4; i++) {
                std::vector<uint8_t> before(ctx->wram, ctx->wram + 8192);
                gb_platform_render_frame(gb_get_framebuffer(ctx));
                if (glGetError() != GL_NO_ERROR || std::memcmp(before.data(), ctx->wram, 8192))
                    std::exit(21);
            }
            return pallet3d_stats();
        };
        auto a = render();
        if (!gb_context_load_state_file(ctx, argv[4]))
            return 5;
        auto b = render();
        if (a.vertices <= b.vertices || b.mesh_builds != a.mesh_builds + 1)
            return 25;
        if (!gb_context_load_state_file(ctx, argv[2]))
            return 5;
        auto restored = render();
        if (restored.vertices != a.vertices || restored.mesh_builds != b.mesh_builds + 1)
            return 26;
        gb_platform_shutdown();
        std::puts("PASS: load Cut result removes geometry, reload original restores it; only "
                  "current mesh rebuilt, RAM read-only");
        return 0;
    }
    if (argc > 5 && std::strcmp(argv[3], "play") == 0) {
        gb_platform_set_input_script(argv[4]);
        int frames = std::atoi(argv[5]);
        for (int frame = 0; frame < frames; frame++) {
            int old_map = pallet::read(ctx, pallet::Map);
            gb_reset_frame(ctx);
            ctx->stopped = 0;
            unsigned slices = 0;
            while (!ctx->frame_done) {
                gb_run_cycles(ctx, 70224);
                if (!gb_platform_poll_events(ctx) || ++slices > 1000)
                    return 12;
            }
            std::vector<uint8_t> before(ctx->wram, ctx->wram + 8192);
            auto view = pallet::view(ctx);
            gb_platform_render_frame(gb_get_framebuffer(ctx));
            if (glGetError() != GL_NO_ERROR || std::memcmp(before.data(), ctx->wram, 8192))
                return 16;
            if (pallet3d_active() !=
                (pallet3d_blend().active || view == pallet::View::Overworld ||
                 view == pallet::View::Battle || pallet3d_warp_overlay() ||
                 pallet3d_battle_transition().active || pallet3d_menu().active ||
                 (view == pallet::View::Dialogue && pallet::bottom_dialogue(ctx)))) {
                std::fprintf(stderr,
                             "[PLAY] wrong scene frame=%d view=%d map=%d active=%d bottom=%d\n",
                             frame, int(view), pallet::read(ctx, pallet::Map), pallet3d_active(),
                             pallet::bottom_dialogue(ctx));
                return 23;
            }
            if (old_map != pallet::read(ctx, pallet::Map))
                std::fprintf(stderr, "[CROSSING] frame=%d %d -> %u xy=%u,%u\n", frame, old_map,
                             pallet::read(ctx, pallet::Map), pallet::read(ctx, pallet::X),
                             pallet::read(ctx, pallet::Y));
        }
        std::fprintf(stderr, "[PLAY] map=%u xy=%u,%u battle=%u party=%u hp=%u font=%u\n",
                     pallet::read(ctx, pallet::Map), pallet::read(ctx, pallet::X),
                     pallet::read(ctx, pallet::Y), pallet::read(ctx, pallet::Battle),
                     pallet::read(ctx, 0xd162),
                     pallet::read(ctx, 0xd16b) * 256 + pallet::read(ctx, 0xd16c),
                     pallet::read(ctx, pallet::Font));
        if (argc > 6 && !gb_context_save_state_file(ctx, argv[6]))
            return 18;
        if (const char *path = std::getenv("SMOKE_CAPTURE"))
            capture_surface(path);
        gb_platform_shutdown();
        return 0;
    }
    if (pallet::view(ctx) != pallet::View::Overworld && pallet::view(ctx) != pallet::View::Battle &&
        !(argc > 3 && std::strcmp(argv[3], "capture2d") == 0))
        return 6;
    if (argc > 3 && std::strcmp(argv[3], "route") == 0) {
        if (pallet::read(ctx, pallet::X) != 9 || pallet::read(ctx, pallet::Y) != 8)
            return 11;
        gb_platform_set_input_script("20:S:4,60:B:4,90:R:60,170:L:64,280:U:100");
        bool saw_menu = false, saw_inside = false, saw_inside_3d = false;
        for (int frame = 0; frame < 450; frame++) {
            gb_reset_frame(ctx);
            ctx->stopped = 0;
            unsigned slices = 0;
            while (!ctx->frame_done) {
                gb_run_cycles(ctx, 70224);
                if (!gb_platform_poll_events(ctx) || ++slices > 1000)
                    return 12;
            }
            auto view = pallet::view(ctx);
            saw_menu |= view == pallet::View::Dialogue;
            saw_inside |= pallet::read(ctx, pallet::Map) == 37;
            if (frame == 155 && pallet::read(ctx, pallet::X) != 9)
                return 13;
            if (frame == 250 &&
                (pallet::read(ctx, pallet::X) != 5 || pallet::read(ctx, pallet::Y) != 8))
                return 14;
            if (pallet::read(ctx, pallet::Map) == 0 && pallet::read(ctx, pallet::Walk) > 0 &&
                pallet::read(ctx, pallet::Font) == 0 && ctx->io[0x47] == 0xe4 &&
                view != pallet::View::Overworld)
                return 15;
            ReadOnlyMemory memory{ctx};
            gb_platform_render_frame(gb_get_framebuffer(ctx));
            if (!memory.unchanged(ctx))
                return 7;
            saw_inside_3d |= pallet::read(ctx, pallet::Map) == 37 && pallet3d_active();
            if (glGetError() != GL_NO_ERROR)
                return 16;
        }
        gb_platform_shutdown();
        if (!saw_menu || !saw_inside || !saw_inside_3d)
            return 17;
        std::puts("PASS: Start menu, return to 3D, wall collision, continuous walking, house "
                  "entrance -> 3D");
        return 0;
    }
    std::vector<uint8_t> before(ctx->wram, ctx->wram + 8192);
    for (int i = 0; i < 35; i++) {
        if (argc > 3 && std::strcmp(argv[3], "camera") == 0 && i == 2) {
            SDL_Event ev{};
            ev.type = SDL_KEYDOWN;
            ev.key.keysym.scancode = SDL_SCANCODE_E;
            for (int j = 0; j < 10; j++)
                pallet3d_event(&ev, false);
            ev.type = SDL_MOUSEWHEEL;
            ev.wheel.y = 4;
            pallet3d_event(&ev, false);
        }
        gb_platform_render_frame(gb_get_framebuffer(ctx));
        auto view = pallet::view(ctx);
        if (pallet3d_active() != (pallet3d_blend().active || view == pallet::View::Overworld ||
                                  view == pallet::View::Battle || pallet3d_warp_overlay() ||
                                  pallet3d_battle_transition().active || pallet3d_menu().active ||
                                  (view == pallet::View::Dialogue && pallet::bottom_dialogue(ctx))))
            return 23;
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::fprintf(stderr, "OpenGL error: %x\n", error);
            return 10;
        }
    }
    if (const char *path = std::getenv("SMOKE_CAPTURE"))
        capture_surface(path);
    if (std::memcmp(before.data(), ctx->wram, before.size()))
        return 7;
    // Check keyboard interception and fallback on the real SDL event path.
    SDL_Event toggle{};
    toggle.type = SDL_KEYDOWN;
    toggle.key.keysym.scancode = SDL_SCANCODE_F2;
    if (!pallet3d_event(&toggle, false))
        return 8;
    gb_platform_render_frame(gb_get_framebuffer(ctx));
    if (!pallet3d_event(&toggle, false))
        return 9;
    gb_platform_render_frame(gb_get_framebuffer(ctx));
    gb_platform_shutdown();
    // No context_destroy: the smoke test must not invoke battery save callbacks.
    std::puts("PASS: real savestate rendered, camera exercised, F2 toggles, WRAM unchanged");
    return 0;
}
