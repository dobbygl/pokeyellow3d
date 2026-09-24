#include "synthetic_context.h"
#include "pallet3d.h"
#include "firstperson.h"
#include "shadow_map_gl.h"
#include "imgui.h"
#include <SDL.h>
#include <SDL_opengles2.h>
#include <cstdio>
#include <cstring>
#include <vector>

// Builds the meshes of the procedural world with pallet3d_preview and draws
// them on a hidden window, checking OpenGL, the mesh statistics, the emulated
// memory and the pixels that reach the surface.
//
// The runtime's own gb_platform_init is deliberately not reused. Its
// render_frame_internal returns before ImGui::NewFrame (and therefore before
// the pallet3d_draw call site) whenever the framebuffer pointer is null, and a
// hand-built GBContext has no PPU, so gb_get_framebuffer would hand it null.
// The same reason makes tests/read_only_memory.h unusable here: it snapshots
// gb_get_framebuffer(ctx). SDL, the ES 2 context and a backend-less ImGui
// context are therefore created below, exactly the pieces pallet3d.cpp needs.
namespace {
constexpr int Width = 640, Height = 480;
constexpr int Skipped = 77;

int skip(const char *reason) {
    const char *detail = SDL_GetError();
    std::fprintf(stderr, "SKIP: %s (driver %s: %s)\n", reason,
                 SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "none",
                 detail && *detail ? detail : "no OpenGL support in the video driver");
    return Skipped;
}
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}

// The emulated machine is presentation input only; nothing may write back.
struct Snapshot {
    std::vector<uint8_t> wram, vram, eram, oam, hram, io, rom;
    std::vector<uint32_t> framebuffer;
    std::array<uint8_t, sizeof(GBContext)> context{};
    explicit Snapshot(const synthetic::Context &m)
        : wram(m.wram.begin(), m.wram.end()), vram(m.vram.begin(), m.vram.end()),
          eram(m.eram.begin(), m.eram.end()), oam(m.oam.begin(), m.oam.end()),
          hram(m.hram.begin(), m.hram.end()), io(m.io.begin(), m.io.end()), rom(m.image),
          framebuffer(m.framebuffer.begin(), m.framebuffer.end()) {
        std::memcpy(context.data(), &m.ctx, context.size());
    }
    bool unchanged(const synthetic::Context &m) const {
        return !std::memcmp(context.data(), &m.ctx, context.size()) &&
               !std::memcmp(wram.data(), m.wram.data(), wram.size()) &&
               !std::memcmp(vram.data(), m.vram.data(), vram.size()) &&
               !std::memcmp(eram.data(), m.eram.data(), eram.size()) &&
               !std::memcmp(oam.data(), m.oam.data(), oam.size()) &&
               !std::memcmp(hram.data(), m.hram.data(), hram.size()) &&
               !std::memcmp(io.data(), m.io.data(), io.size()) && rom == m.image &&
               !std::memcmp(framebuffer.data(), m.framebuffer.data(), framebuffer.size() * 4);
    }
};
} // namespace

int main() {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        return skip("SDL video initialization failed");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    // pallet3d clears and tests depth; the runtime attributes callback requests the same value.
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_Window *window = SDL_CreateWindow("pallet3d synthetic preview", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, Width, Height,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window) {
        SDL_Quit();
        return skip("no OpenGL window on this video driver");
    }
    SDL_GLContext gl = SDL_GL_CreateContext(window);
    if (!gl) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return skip("no OpenGL ES 2 context on this video driver");
    }
    std::fprintf(stderr, "[SYNTH] driver=%s renderer=%s\n", SDL_GetCurrentVideoDriver(),
                 reinterpret_cast<const char *>(glGetString(GL_RENDERER)));

    // A bare ImGui context: pallet3d only asks for GetIO().DeltaTime and the
    // foreground draw list, and the draw data is never submitted to OpenGL.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(float(Width), float(Height));
    io.DeltaTime = 1.f / 60;
    unsigned char *font_pixels = nullptr;
    int font_width = 0, font_height = 0;
    io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
    io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(intptr_t(1)));

    int status = 0;
    try {
        const auto &plan = synthetic::layout();
        synthetic::Context machine;
        GBContext *ctx = &machine.ctx;
        machine.reset();
        const auto original_image = machine.image;
        machine.place_player(plan.home, 4, 4, 0);
        check(pallet::view(ctx) == pallet::View::Overworld,
              "the fixture starts on a live overworld");
        // presented_scene() looks the map up without instantiating it.
        check(pallet::ensure_scene(plan.interior) != nullptr,
              "the interior is resident before its preview");
        // The outdoor atlas always samples tilesets 0, 3, 14 and 23. The
        // procedural world has no map on the plateau tileset, which the
        // cartridge reaches through Route 23 inside Pallet Town's connected
        // component, so it is registered here. Its header is valid in the
        // generated image like every other tileset id.
        pallet::catalog->tilesets.emplace(
            23, kanto::read_tileset(kanto::Rom(ctx->rom, ctx->rom_size), 23));

        std::vector<uint8_t> rgba(size_t(Width) * Height * 4);
        auto frame = [&] {
            io.DeltaTime = 1.f / 60;
            ImGui::NewFrame();
            pallet3d_draw(ctx, Width, Height, false);
            ImGui::EndFrame();
        };
        // Reading before any buffer swap keeps the back buffer defined.
        auto preview = [&](int map, const char *label) {
            pallet3d_preview(map);
            Snapshot before{machine};
            for (int i = 0; i < 10; i++) {
                frame();
                check(glGetError() == GL_NO_ERROR, "a frame raised an OpenGL error");
                check(pallet3d_active(), "the preview must present a 3D frame");
                check(before.unchanged(machine), "presentation wrote to the emulated machine");
            }
            auto stats = pallet3d_stats();
            check(stats.resident_maps == 1, "a preview keeps exactly one mesh resident");
            check(stats.vertices > 0, "the preview mesh has geometry");
            glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            check(glGetError() == GL_NO_ERROR, "reading the surface raised an OpenGL error");
            // The clear colour fills the corners; drawn geometry must not.
            size_t drawn = 0, shades = 0;
            bool seen[4096] = {};
            for (size_t i = 0; i < rgba.size(); i += 4) {
                if (std::memcmp(&rgba[i], rgba.data(), 3))
                    ++drawn;
                int key = (rgba[i] >> 4) << 8 | (rgba[i + 1] >> 4) << 4 | (rgba[i + 2] >> 4);
                if (!seen[key]) {
                    seen[key] = true;
                    ++shades;
                }
            }
            std::fprintf(stderr, "[SYNTH] %s map=%d vertices=%zu bytes=%zu drawn=%zu shades=%zu\n",
                         label, map, stats.vertices, stats.bytes, drawn, shades);
            check(drawn > rgba.size() / 4 / 20, "the surface is still the background colour");
            check(shades > 4, "the surface carries no shaded geometry");
        };

        pallet3d_artistic(false);
        preview(plan.home, "outdoor");
        auto disabled = rgba;
        const auto reference_vertices = pallet3d_stats().vertices;
        // C1 changes silhouettes. OFF must recover the image, and each toggle
        // must rebuild once, then reuse the mesh on an unchanged frame.
        for (bool artistic : {true, false, true, false}) {
            const auto builds = pallet3d_stats().mesh_builds;
            check(pallet3d_artistic(artistic), "toggle artistic presentation");
            Snapshot unchanged{machine};
            frame();
            glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            check(artistic ? rgba != disabled : rgba == disabled,
                  "C1 silhouettes change only when enabled and OFF restores every pixel");
            check(pallet3d_stats().mesh_builds == builds + 1,
                  "an art toggle rebuilds the resident mesh exactly once");
            check(pallet3d_stats().vertices <= 2 * reference_vertices,
                  "procedural geometry stays inside the 2x vertex budget");
            frame();
            check(pallet3d_stats().mesh_builds == builds + 1, "an unchanged frame reuses its mesh");
            check(unchanged.unchanged(machine), "art toggle wrote to the emulated machine");
            check(glGetError() == GL_NO_ERROR, "art toggle raised an OpenGL error");
        }
        // Independent B1 lighting oracle: the same synthetic sign geometry in
        // both modes. C1 tree/rock silhouettes must not invalidate its pixel
        // bounds or allow a shadow bug to hide behind changed geometry.
        pallet3d_shutdown();
        for (int block : {1, 2, 3, 4, 6, 8})
            for (int tile = 0; tile < 16; ++tile)
                machine.image[plan.outdoor_block_table + size_t(block) * 16 + tile] =
                    (tile / 4) % 2 ? 0x56 : 0x46;
        pallet::load_catalog(machine.image.data(), machine.image.size());
        pallet::catalog->tilesets.emplace(
            23, kanto::read_tileset(kanto::Rom(ctx->rom, ctx->rom_size), 23));
        preview(plan.home, "shadow oracle signs");
        disabled = rgba;
        auto lit = [&](double hour) {
            check(pallet3d_daylight({daynight::Mode::Fixed, hour}), "set fixed presentation hour");
            Snapshot before{machine};
            frame();
            check(before.unchanged(machine), "lighting wrote to the emulated machine");
            check(glGetError() == GL_NO_ERROR, "lighting raised an OpenGL error");
            auto light = pallet3d_daylight_frame();
            check(light.enabled && light.hour == hour,
                  "rendered frame contains requested lighting");
            glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            return rgba;
        };
        auto noon = lit(12), night = lit(0);
        check(noon != night && noon != disabled && night != disabled,
              "actual GPU pixels respond to time of day");
        const auto old_passes = pallet3d_shadows().passes;
        check(pallet3d_artistic(true), "enable artistic shadows");
        auto shadowed = lit(12);
        auto shadow_info = pallet3d_shadows();
        check(shadow_info.ready && shadow_info.active && shadow_info.artistic_scene &&
                  shadow_info.passes == old_passes + 1,
              "daylight submits the actual shadow target");
        size_t darker = 0;
        const auto ambient = daynight::sample({daynight::Mode::Fixed, 12}, 0).ambient;
        for (size_t i = 0; i < shadowed.size(); i += 4) {
            if (!std::memcmp(shadowed.data() + i, noon.data() + i, 3))
                continue;
            ++darker;
            for (size_t channel = 0; channel < 3; ++channel) {
                check(shadowed[i + channel] <= noon[i + channel], "a shadow cannot add light");
                // Orthographic, noon, no emissive windows or fog: the original
                // unlit pixel times ambient is an independent lower bound.
                // Two byte units cover both normalized-byte roundings.
                check(shadowed[i + channel] + 2 >= disabled[i + channel] * ambient[channel],
                      "shadow incorrectly attenuates ambient light");
            }
        }
        check(darker > 0, "GPU shadow map must darken real visible geometry");
        // Without sun no shadow pass runs. The only artistic change at night is
        // static vertex AO: it darkens and never below four occluded probes.
        const auto night_passes = pallet3d_shadows().passes;
        const auto artistic_night = lit(0);
        check(pallet3d_shadows().passes == night_passes, "night submits no shadow pass");
        for (size_t i = 0; i < night.size(); i += 4)
            for (size_t channel = 0; channel < 3; ++channel) {
                check(artistic_night[i + channel] <= night[i + channel],
                      "artistic pass cannot add light at night");
                check(artistic_night[i + channel] + 2 >= night[i + channel] * .68f,
                      "night darkening is bounded by vertex AO");
            }
        check(!pallet3d_shadows().active && pallet3d_shadows().passes == old_passes + 1,
              "night does not submit a depth pass");
        check(pallet3d_artistic(false), "disable artistic shadows");
        check(lit(12) == noon, "disabling shadows restores every reference daylight pixel");
        check(pallet3d_shadows().passes == old_passes + 1,
              "disabled artistic pass does not submit depth");
        check(pallet3d_daylight({}), "disable cycle");
        frame();
        glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        check(rgba == disabled, "disabled cycle recovers every original pixel");
        preview(plan.interior, "interior");
        // Isolate B2 from silhouettes, furniture contacts and sunlight: an
        // empty synthetic room has only floor and perimeter walls. Every
        // pixel change must therefore be local occlusion of existing faces.
        pallet3d_shutdown();
        const size_t indoor_blocks = synthetic::detail::IndoorBlocks;
        std::copy_n(machine.image.begin() + indoor_blocks, 16,
                    machine.image.begin() + indoor_blocks + 2 * 16);
        pallet::catalog.reset();
        check(pallet::load_catalog(ctx->rom, ctx->rom_size), "load empty room AO oracle");
        check(pallet::ensure_scene(plan.interior) != nullptr, "load AO room");
        for (bool fp : {false, true}) {
            pallet3d_artistic(false);
            pallet3d_preview(plan.interior, fp);
            frame();
            glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            const auto room_off = rgba;
            const auto room_vertices = pallet3d_stats().vertices;
            const auto builds = pallet3d_stats().mesh_builds;
            pallet3d_artistic(true);
            Snapshot unchanged{machine};
            frame();
            glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            size_t ao_pixels = 0, unchanged_pixels = 0;
            for (size_t i = 0; i < rgba.size(); i += 4) {
                bool dark = false;
                for (int c = 0; c < 3; ++c) {
                    check(rgba[i + c] <= room_off[i + c], "AO cannot brighten an empty room");
                    dark |= rgba[i + c] < room_off[i + c];
                }
                ao_pixels += dark;
                unchanged_pixels += !dark;
            }
            check(ao_pixels > 0 && unchanged_pixels > 0, "AO reaches visible room pixels locally");
            check(pallet3d_stats().vertices == room_vertices,
                  "vertex AO adds no geometry in an empty room");
            for (int i = 0; i < 5; ++i) {
                frame();
                check(pallet3d_stats().mesh_builds == builds + 1,
                      "AO is built once and reused on subsequent frames");
                check(unchanged.unchanged(machine) && glGetError() == GL_NO_ERROR,
                      "AO frames keep guest memory and GL intact");
            }
            pallet3d_artistic(false);
            frame();
            glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            check(rgba == room_off, "disabling AO restores every room pixel");
        }
        // A2 geometry oracle: the old classifier leaves a procedural machine
        // block flat. AO alone cannot increase its vertex count. Keep every
        // other cell unchanged and exercise both cameras and the FBO fallback.
        pallet3d_shutdown();
        const auto room_header = pallet::scene(plan.interior)->header;
        std::copy_n(machine.image.begin() + kanto::TilesetHeaders + 12, 12,
                    machine.image.begin() + kanto::TilesetHeaders + 22 * 12);
        machine.image[room_header] = 22;
        for (int i = 0; i < 16; ++i)
            machine.image[indoor_blocks + 2 * 16 + i] = i / 4 % 2 ? 0x4c : 0x4a;
        pallet::catalog.reset();
        check(pallet::load_catalog(ctx->rom, ctx->rom_size), "load A2 machine fixture");
        check(pallet::ensure_scene(plan.interior) != nullptr, "load A2 room");
        for (bool fp : {false, true}) {
            pallet3d_shutdown();
            pallet3d_artistic(false);
            pallet3d_preview(plan.interior, fp);
            frame();
            glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            const auto off = rgba;
            const auto flat_vertices = pallet3d_stats().vertices;
            const auto builds = pallet3d_stats().mesh_builds;
            Snapshot unchanged{machine};
            pallet3d_artistic(true);
            frame();
            glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            check(pallet3d_stats().vertices > flat_vertices && rgba != off,
                  "A2 creates visible machine geometry, not only ambient shading");
            frame();
            check(pallet3d_stats().mesh_builds == builds + 1,
                  "A2 toggle rebuilds once and stable frames reuse the mesh");
            pallet3d_artistic(false);
            frame();
            glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            check(rgba == off && pallet3d_stats().vertices == flat_vertices,
                  "A2 OFF restores every pixel and the original flat geometry");
            check(unchanged.unchanged(machine) && glGetError() == GL_NO_ERROR,
                  "A2 toggles preserve guest state and OpenGL");

            pallet3d_shutdown();
            check(!shadow_map::initialize([](GLenum, const char *) -> GLuint { return 0; },
                                          [](GLuint, GLuint) {}),
                  "A2 negative case fails a real incomplete shadow framebuffer");
            pallet3d_artistic(true);
            pallet3d_preview(plan.interior, fp);
            frame();
            glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            check(rgba == off && pallet3d_stats().vertices == flat_vertices &&
                      !pallet3d_shadows().artistic_scene,
                  "A2 FBO failure falls back completely, including new classified objects");
            check(unchanged.unchanged(machine) && glGetError() == GL_NO_ERROR,
                  "A2 fallback preserves guest state and OpenGL");
        }
        // Visibility oracle on a walkable cell immediately behind the new
        // machine block. A procedural opaque sprite must still reach the GPU
        // surface from each room angle (including the existing x-ray path).
        pallet3d_shutdown();
        constexpr size_t SpriteCommands = 0x3000;
        machine.image[pallet::FacingTable] = SpriteCommands & 255;
        machine.image[pallet::FacingTable + 1] = SpriteCommands >> 8;
        machine.image[SpriteCommands] = 4;
        for (int i = 0; i < 4; ++i) {
            const size_t command = SpriteCommands + 1 + size_t(i) * 4;
            machine.image[command] = uint8_t(i / 2 * 8);
            machine.image[command + 1] = uint8_t(i % 2 * 8);
            machine.image[command + 2] = uint8_t(i);
            machine.image[command + 3] = 0;
        }
        machine.place_player(plan.interior, 4, 3, 0);
        pallet3d_artistic(true);
        pallet3d_preview(-1);
        frame();
        pallet3d_poll_controls(ctx, false);
        SDL_Event turn{};
        turn.type = SDL_KEYDOWN;
        turn.key.keysym.scancode = SDL_SCANCODE_Q;
        for (int i = 0; i < 10; ++i)
            check(pallet3d_event(&turn, false), "room rotation handles the camera key");
        turn.key.keysym.scancode = SDL_SCANCODE_E;
        for (int angle = 0; angle < 4; ++angle) {
            machine.vram.fill(0);
            frame();
            glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            const auto without_sprite = rgba;
            for (size_t byte = 0; byte < 4 * 16; byte += 2)
                machine.vram[byte] = 255;
            Snapshot unchanged{machine};
            frame();
            glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            size_t visible_pixels = 0;
            for (size_t i = 0; i < rgba.size(); i += 4)
                visible_pixels += std::memcmp(rgba.data() + i, without_sprite.data() + i, 3) != 0;
            check(visible_pixels > 20 && pallet3d_camera().player_drawn,
                  "player remains visible beside A2 furniture from every tested room angle");
            check(unchanged.unchanged(machine) && glGetError() == GL_NO_ERROR,
                  "A2 player visibility frame preserves guest and OpenGL");
            for (int i = 0; i < 3; ++i)
                check(pallet3d_event(&turn, false), "room rotation remains available");
        }
        std::copy(original_image.begin(), original_image.end(), machine.image.begin());
        machine.vram.fill(0);
        machine.place_player(plan.home, 4, 4, 0);
        // Restore the television block before the remaining live/fallback QA.
        pallet3d_shutdown();
        std::copy_n(original_image.begin() + indoor_blocks + 2 * 16, 16,
                    machine.image.begin() + indoor_blocks + 2 * 16);
        pallet::catalog.reset();
        check(pallet::load_catalog(ctx->rom, ctx->rom_size), "restore room fixture");
        pallet::catalog->tilesets.emplace(
            23, kanto::read_tileset(kanto::Rom(ctx->rom, ctx->rom_size), 23));
        preview(plan.east, "neighbour");

        // Back to the live game: the overworld state above is drawn instead.
        pallet3d_preview(-1);
        Snapshot before{machine};
        for (int i = 0; i < 3; i++) {
            frame();
            check(glGetError() == GL_NO_ERROR, "returning to the live view raised an OpenGL error");
            check(before.unchanged(machine), "the live view wrote to the emulated machine");
        }
        check(pallet3d_active(), "the live overworld is presented in 3D as well");
        check(pallet3d_stats().vertices > 0, "the live overworld keeps its mesh");
        {
            // AO invalidation follows the halo: a live block change far from
            // every seam rebuilds only its own map; one on the northern seam
            // also rebuilds the northern neighbour, never the eastern one.
            pallet3d_artistic(true);
            auto settle = [&] {
                // A changed block is staged for one frame before it is used.
                for (int i = 0; i < 3; i++)
                    frame();
            };
            settle();
            check(pallet3d_stats().resident_maps == 3, "home and both neighbours are resident");
            auto live_block = [](int x, int z) { return 0xc6e8 + (z + 3) * (5 + 6) + x + 3; };
            auto builds = pallet3d_stats().mesh_builds;
            settle();
            check(pallet3d_stats().mesh_builds == builds, "a static live scene is never rebuilt");
            const int centre = machine.at(live_block(2, 2)), corner = machine.at(live_block(0, 0));
            machine.write(live_block(2, 2), centre == 1 ? 2 : 1);
            settle();
            std::fprintf(stderr, "[SYNTH] AO rebuilds after a central block change: %zu\n",
                         pallet3d_stats().mesh_builds - builds);
            check(pallet3d_stats().mesh_builds == builds + 1,
                  "a change far from every seam rebuilds only its own map");
            builds = pallet3d_stats().mesh_builds;
            machine.write(live_block(0, 0), corner == 1 ? 2 : 1);
            settle();
            std::fprintf(stderr, "[SYNTH] AO rebuilds after a northern seam change: %zu\n",
                         pallet3d_stats().mesh_builds - builds);
            check(pallet3d_stats().mesh_builds == builds + 2,
                  "a seam change rebuilds its map and the neighbour reading it");
            machine.write(live_block(2, 2), centre);
            machine.write(live_block(0, 0), corner);
            settle();
            pallet3d_artistic(false);
            settle();
        }
        // Inject a real allocation failure before the production renderer's
        // lazy initialization. Compare the complete scene, not just the FBO
        // helper's return value, in both styles/cameras and across re-entry.
        // Restore the C1 trees/rocks as well: a sign-only fixture would not
        // catch a partial geometry fallback after the shadow FBO failed.
        pallet3d_shutdown();
        std::copy(original_image.begin(), original_image.end(), machine.image.begin());
        pallet::catalog.reset();
        check(pallet::load_catalog(ctx->rom, ctx->rom_size), "restore C1 fallback fixture");
        pallet::catalog->tilesets.emplace(
            23, kanto::read_tileset(kanto::Rom(ctx->rom, ctx->rom_size), 23));
        for (auto style : {ui_preferences::Style::Classic, ui_preferences::Style::Integrated})
            for (bool fp : {false, true}) {
                std::vector<uint8_t> reference;
                std::vector<ImDrawVert> hud_reference;
                for (bool fail : {false, true}) {
                    pallet3d_shutdown();
                    pallet3d_menu_style(style);
                    pallet3d_artistic(fail);
                    pallet3d_daylight({daynight::Mode::Fixed, 12});
                    if (fail) {
                        bool incomplete = false, compiled = false;
                        check(!shadow_map::initialize(
                                  [&](GLenum, const char *) -> GLuint {
                                      compiled = true;
                                      return 0;
                                  },
                                  [&](GLuint color, GLuint depth) {
                                      check(color && depth,
                                            "negative case allocated real resources");
                                      incomplete = glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
                                                   GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
                                  }) &&
                                  incomplete && !compiled,
                              "real incomplete target failed before shader compilation");
                    }
                    pallet3d_preview(plan.home, fp);
                    Snapshot unchanged{machine};
                    for (int repeat = 0; repeat < 10; ++repeat) {
                        frame();
                        check(unchanged.unchanged(machine), "fallback changed guest state");
                        check(glGetError() == GL_NO_ERROR, "fallback frame raised GL error");
                        check(pallet3d_active() && !pallet3d_shadows().artistic_scene &&
                                  !pallet3d_shadows().active && !pallet3d_shadows().passes,
                              "the whole scene must use reference policy, never partial art");
                    }
                    glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
                    auto &hud = ImGui::GetForegroundDrawList()->VtxBuffer;
                    if (!fail) {
                        reference = rgba;
                        hud_reference.assign(hud.begin(), hud.end());
                    } else {
                        check(pallet3d_artistic(),
                              "fallback does not change the user's preference");
                        check(rgba == reference,
                              "failed target restores every reference scene byte");
                        check(hud_reference.size() == size_t(hud.Size) &&
                                  (hud.empty() ||
                                   !std::memcmp(hud_reference.data(), hud.Data,
                                                hud_reference.size() * sizeof(ImDrawVert))),
                              "fallback preserves HUD vertices, colours and glyph coordinates");
                        check(shadow_map::attempted && !shadow_map::ready && !shadow_map::texture &&
                                  !shadow_map::framebuffer && !shadow_map::depth &&
                                  !shadow_map::program,
                              "failed shadow resources stay released across scene re-entry");
                    }
                }
            }
        // Independent acne oracle: an entirely flat synthetic map has no
        // object above the receiving plane, so it cannot cast a shadow onto
        // itself. This catches sampling/bias errors that still darken pixels
        // within the ambient bound and therefore pass the earlier test.
        pallet3d_shutdown();
        synthetic::Context plane;
        for (int block = 0; block < 256; ++block)
            for (int tile = 0; tile < 16; ++tile)
                plane.image[plan.outdoor_block_table + size_t(block) * 16 + tile] =
                    (tile / 4) % 2 ? plan.ground_bottom : plan.ground_top;
        ctx = &plane.ctx;
        plane.place_player(plan.home, 4, 4, 0);
        pallet::catalog->tilesets.emplace(
            23, kanto::read_tileset(kanto::Rom(ctx->rom, ctx->rom_size), 23));
        for (bool fp : {false, true}) {
            pallet3d_preview(plan.home, fp);
            for (double hour : {6.5, 9., 12., 15., 17.5}) {
                check(pallet3d_daylight({daynight::Mode::Fixed, hour}), "set planar test sun");
                std::vector<uint8_t> reference;
                for (bool artistic : {false, true}) {
                    pallet3d_artistic(artistic);
                    Snapshot unchanged{plane};
                    frame();
                    check(unchanged.unchanged(plane),
                          "planar shadow test changed synthetic memory");
                    check(glGetError() == GL_NO_ERROR, "planar shadow test raised GL error");
                    glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
                    if (!artistic)
                        reference = rgba;
                    else {
                        check(pallet3d_shadows().active, "flat fixture exercises shadow sampling");
                        firstperson::Matrix camera;
                        if (fp) {
                            const auto info = pallet3d_camera();
                            firstperson::Camera eye;
                            eye.x = info.x;
                            eye.y = info.y;
                            eye.z = info.z;
                            eye.yaw = info.yaw;
                            camera = eye.perspective(float(Width) / Height);
                        } else {
                            const float c = std::cos(-.32f), s = std::abs(std::sin(-.32f));
                            float unit = std::min(Width / (10 * (c + s) + 5),
                                                  Height / (10 * (s + c) * .78f + 6));
                            camera = firstperson::orthographic(5, 5, -.32f, 2 * unit / Width,
                                                               2 * unit / Height);
                        }
                        size_t checked = 0;
                        for (size_t pixel = 0; pixel < rgba.size(); ++pixel) {
                            // Intersect this camera ray with y=0 independently
                            // of the shadow fit. Exclude only the map's outer
                            // half-cell: PCF can blend the slab silhouette there.
                            double u = (pixel / 4 % Width + .5) * 2 / Width - 1;
                            double v = (pixel / 4 / Width + .5) * 2 / Height - 1;
                            double a = camera[0] - u * camera[3], b = camera[8] - u * camera[11];
                            double c = camera[1] - v * camera[3], d = camera[9] - v * camera[11];
                            double e = u * camera[15] - camera[12], f = v * camera[15] - camera[13];
                            double determinant = a * d - b * c;
                            if (std::abs(determinant) < 1e-10)
                                continue;
                            double x = (e * d - b * f) / determinant;
                            double z = (a * f - e * c) / determinant;
                            if (x < .5 || x > 9.5 || z < .5 || z > 9.5 ||
                                camera[3] * x + camera[11] * z + camera[15] <= 0)
                                continue;
                            ++checked;
                            if (std::abs(int(rgba[pixel]) - int(reference[pixel])) > 1)
                                std::fprintf(stderr,
                                             "[PLANE] fp=%d hour=%.2f pixel=%zu,%zu channel=%zu "
                                             "reference=%u shadow=%u\n",
                                             fp, hour, pixel / 4 % Width, pixel / 4 / Width,
                                             pixel % 4, unsigned(reference[pixel]),
                                             unsigned(rgba[pixel]));
                            check(std::abs(int(rgba[pixel]) - int(reference[pixel])) <= 1,
                                  "unoccluded flat surface has false shadow bands");
                        }
                        check(checked > 4000, "planar acne oracle covers a visible floor region");
                    }
                }
            }
        }
        std::fprintf(stderr, "PASS: 30 preview frames over three maps plus the live overworld, "
                             "OpenGL clean, memory untouched, real shadows and no planar acne\n");
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        status = 1;
    }

    pallet3d_shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return status;
}
