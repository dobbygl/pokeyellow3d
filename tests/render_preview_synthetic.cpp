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
        check(lit(0) == night, "night stays byte-exact with the artistic pass on");
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
