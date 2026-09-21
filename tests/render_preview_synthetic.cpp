#include "synthetic_context.h"
#include "pallet3d.h"
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
    std::vector<uint8_t> wram, vram, eram;
    std::vector<uint32_t> framebuffer;
    explicit Snapshot(const synthetic::Context &m)
        : wram(m.wram.begin(), m.wram.end()), vram(m.vram.begin(), m.vram.end()),
          eram(m.eram.begin(), m.eram.end()),
          framebuffer(m.framebuffer.begin(), m.framebuffer.end()) {}
    bool unchanged(const synthetic::Context &m) const {
        return !std::memcmp(wram.data(), m.wram.data(), wram.size()) &&
               !std::memcmp(vram.data(), m.vram.data(), vram.size()) &&
               !std::memcmp(eram.data(), m.eram.data(), eram.size()) &&
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

        preview(plan.home, "outdoor");
        const auto disabled = rgba;
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
        std::fprintf(stderr, "PASS: 30 preview frames over three maps plus the live overworld, "
                             "OpenGL clean, memory untouched and a non-empty surface\n");
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
