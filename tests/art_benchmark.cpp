// Compile this same driver against the frozen reference libraries with
// ART_REFERENCE_RENDERER, and against the candidate normally. No fixture ships.
extern "C" {
#include "pokeyellow.h"
}
#include "platform_sdl.h"
#include "pallet3d.h"
#include "pallet_state.h"
#include "assets_manifest_pokeyellow.h"
#include "read_only_memory.h"
#include "qa_presentation_clock.h"
#include <SDL_opengles2.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
extern "C" uint8_t pokeyellow__rom_data[];

int main(int argc, char **argv) {
    if (argc != 6) {
        std::fprintf(stderr, "Usage: art_benchmark ROM STATE catalog|interior-catalog|ortho|fp "
                             "off|on HOUR(-1 disables)\n");
        return 2;
    }
    const bool art = !std::strcmp(argv[4], "on");
    if (std::strcmp(argv[4], "off") && !art)
        return 2;
#ifdef ART_REFERENCE_RENDERER
    if (art) {
        std::fprintf(stderr, "Reference renderer cannot enable the artistic pass\n");
        return 2;
    }
#endif
    const std::string mode = argv[3];
    const bool catalog = mode == "catalog" || mode == "interior-catalog";
    if (!catalog && mode != "ortho" && mode != "fp")
        return 2;
    char *end = nullptr;
    double hour = std::strtod(argv[5], &end);
    if (end == argv[5] || *end || !std::isfinite(hour) || (hour != -1 && (hour < 0 || hour >= 24)))
        return 2;
    std::ifstream file(argv[1], std::ios::binary);
    std::ifstream fixture(argv[2], std::ios::binary);
    if (!file || !fixture) {
        std::fprintf(stderr, "SKIP: private ROM or benchmark savestate missing\n");
        return 77;
    }
    std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file), {}};
    if (rom.size() != 1048576)
        return 3;
    std::memset(pokeyellow__rom_data, 0xff, 1048576);
    for (auto entry : POKEYELLOW_ASSETS_MANIFEST)
        std::memcpy(pokeyellow__rom_data + entry.rom_offset, rom.data() + entry.rom_offset,
                    entry.size);
    SDL_SetMainReady();
    GBConfig config = *pokeyellow_default_config();
    config.model = config.cartridge_supports_cgb ? GB_MODEL_CGB : GB_MODEL_DMG;
    config.cgb_compatibility_mode = false;
    GBContext *ctx = gb_context_create(&config);
    if (!ctx || !qa_clock::install() || !gb_platform_init(5))
        return 4;
    struct Shutdown {
        ~Shutdown() {
            gb_platform_shutdown();
        }
    } shutdown;
    gb_platform_register_context(ctx);
    gb_platform_set_game_id(ctx, "pokeyellow");
    pokeyellow_init(ctx);
    if (!gb_context_load_state_file(ctx, argv[2])) {
        return 5;
    }
    pallet3d_menu_style(ui_preferences::Style::Integrated);
    pallet3d_daylight(
        {hour < 0 ? daynight::Mode::Disabled : daynight::Mode::Fixed, hour < 0 ? 12 : hour});
#ifndef ART_REFERENCE_RENDERER
    pallet3d_artistic(art);
#endif
    if (mode == "fp") {
        SDL_Event key{};
        key.type = SDL_KEYDOWN;
        key.key.keysym.scancode = SDL_SCANCODE_F3;
        if (!pallet3d_event(&key, false) || !pallet3d_firstperson()) {
            return 6;
        }
    }
    if (!pallet::load_catalog(ctx->rom, ctx->rom_size))
        return 7;
    std::vector<int> maps;
    if (catalog) {
        if (mode == "interior-catalog")
            pallet::catalog->discover_warps(kanto::Rom(ctx->rom, ctx->rom_size));
        for (const auto &entry : pallet::catalog->maps)
            if (mode != "interior-catalog" || entry.second.interior)
                maps.push_back(entry.first);
    } else {
        maps.push_back(-1);
    }
    ReadOnlyMemory before{ctx};
    const std::vector<uint8_t> immutable_rom(ctx->rom, ctx->rom + ctx->rom_size);
    const auto cycles = ctx->cycles;
    const auto pc = ctx->pc, sp = ctx->sp;
    auto render = [&](double *elapsed = nullptr) {
        auto start = std::chrono::steady_clock::now();
        gb_platform_render_frame(gb_get_framebuffer(ctx));
        glFinish();
        auto end = std::chrono::steady_clock::now();
        if (elapsed)
            *elapsed += std::chrono::duration<double, std::milli>(end - start).count();
        // Guard cost is outside the presentation measurement in both builds.
        return glGetError() == GL_NO_ERROR && before.unchanged(ctx) && pallet3d_active() &&
               ctx->cycles == cycles && ctx->pc == pc && ctx->sp == sp &&
               !std::memcmp(immutable_rom.data(), ctx->rom, immutable_rom.size());
    };
    std::printf("[ART-GPU] %s\n", glGetString(GL_RENDERER));
    for (int id : maps) {
        if (catalog)
            pallet3d_preview(id);
        for (int i = 0; i < 15; ++i)
            if (!render())
                return 8;
        auto stats = pallet3d_stats();
        if (stats.resident_maps != (catalog ? 1u : 5u) || !stats.vertices)
            return 9;
        const int count = catalog ? 30 : 100;
        const int batches = catalog ? 3 : 7;
        for (int sample = 0; sample < batches; ++sample) {
            double elapsed = 0;
            for (int i = 0; i < count; ++i)
                if (!render(&elapsed))
                    return 10;
            double mean = elapsed / count;
            std::printf("[ART-PERF] mode=%s map=%d sample=%d frames=%d mean_ms=%.6f vertices=%zu\n",
                        mode.c_str(), id, sample, count, mean, stats.vertices);
        }
    }
    std::puts("PASS: paired presentation benchmark, memory intact and zero GL errors per frame");
    return 0;
}
