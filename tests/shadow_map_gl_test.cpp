#include <SDL.h>
#include <SDL_opengles2.h>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
bool omit_attachments = false;
void color_attachment(GLenum target, GLenum attachment, GLenum type, GLuint texture, GLint level) {
    if (!omit_attachments)
        glFramebufferTexture2D(target, attachment, type, texture, level);
}
void depth_attachment(GLenum target, GLenum attachment, GLenum type, GLuint renderbuffer) {
    if (!omit_attachments)
        glFramebufferRenderbuffer(target, attachment, type, renderbuffer);
}
} // namespace
// Actual missing-attachment FBO, not a manufactured status code. The production
// header is compiled in a private test namespace, with no game fault switch.
#define shadow_map shadow_test_map
#define glFramebufferTexture2D color_attachment
#define glFramebufferRenderbuffer depth_attachment
#include "shadow_map_gl.h"
#undef glFramebufferTexture2D
#undef glFramebufferRenderbuffer
#undef shadow_map
namespace shadow_map = shadow_test_map;

namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
GLuint compile(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char message[2048]{};
        glGetShaderInfoLog(shader, sizeof(message), nullptr, message);
        std::fprintf(stderr, "%s\n", message);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
} // namespace
int main() {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SKIP: SDL video unavailable: %s\n", SDL_GetError());
        return 77;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_Window *window =
        SDL_CreateWindow("Shadow target test", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    SDL_GLContext context = window ? SDL_GL_CreateContext(window) : nullptr;
    if (!context) {
        std::fprintf(stderr, "SKIP: GLES2 unavailable: %s\n", SDL_GetError());
        if (window)
            SDL_DestroyWindow(window);
        SDL_Quit();
        return 77;
    }
    int result = 0;
    GLuint atlas = 0, vertices = 0, readback = 0;
    try {
        omit_attachments = true;
        check(!shadow_map::initialize(compile), "an incomplete FBO must reject artistic rendering");
        check(!shadow_map::ready && !shadow_map::texture && !shadow_map::framebuffer &&
                  !shadow_map::depth && !shadow_map::program,
              "failed FBO must release all partial resources");
        check(glGetError() == GL_NO_ERROR, "fallback leaves no GL error");
        omit_attachments = false;
        check(!shadow_map::initialize(compile),
              "failed target stays rejected until lifecycle reset");
        shadow_map::shutdown();
        check(shadow_map::initialize(compile), "valid RGBA8 target and program initialize");
        // White RGB with one transparent quadrant; texture alpha is the oracle.
        const unsigned char pixels[]{255, 255, 255, 0,   255, 255, 255, 255,
                                     255, 255, 255, 255, 255, 255, 255, 255};
        glGenTextures(1, &atlas);
        glBindTexture(GL_TEXTURE_2D, atlas);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // position.xyz + uv.xy, full clip square at depth exactly 0.5.
        const float quad[]{-1, -1, 0, 0, 0, 1, -1, 0, 1, 0, 1,  1, 0, 1, 1,
                           -1, -1, 0, 0, 0, 1, 1,  0, 1, 1, -1, 1, 0, 0, 1};
        glGenBuffers(1, &vertices);
        glBindBuffer(GL_ARRAY_BUFFER, vertices);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        sunlight::Projection projection;
        projection.valid = true;
        projection.matrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        auto draw = [&](float alpha) {
            shadow_map::render(projection, 0, atlas, [&] {
                glEnableVertexAttribArray(0);
                glEnableVertexAttribArray(1);
                glDisableVertexAttribArray(2);
                glDisableVertexAttribArray(3);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                                      reinterpret_cast<void *>(3 * sizeof(float)));
                glVertexAttrib4f(2, 1, 1, 1, alpha);
                glVertexAttrib1f(3, 0);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            });
            check(glGetError() == GL_NO_ERROR, "depth pass raised a GL error");
        };
        glEnable(GL_DITHER);
        draw(1);
        check(glIsEnabled(GL_DITHER), "depth pass restores dithering");
        GLint bound = -1;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &bound);
        check(bound == 0, "depth pass restores the caller framebuffer");
        glGenFramebuffers(1, &readback);
        auto read = [&] {
            glBindFramebuffer(GL_FRAMEBUFFER, readback);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   shadow_map::texture, 0);
            check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                  "RGBA depth bytes must be readable");
            std::vector<unsigned char> data(1024 * 1024 * 4);
            glReadPixels(0, 0, 1024, 1024, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            check(glGetError() == GL_NO_ERROR, "readback raised a GL error");
            return data;
        };
        auto depth = read();
        for (int y = 0; y < 1024; ++y)
            for (int x = 0; x < 1024; ++x) {
                const auto *pixel = &depth[size_t(y * 1024 + x) * 4];
                if (x < 512 && y < 512)
                    check(pixel[0] == 255 && pixel[1] == 255 && pixel[2] == 255,
                          "transparent sprite padding must not cast");
                else {
                    double z = pixel[0] / 255. + pixel[1] / 65025. + pixel[2] / 16581375.;
                    check(std::abs(z - .5) < 2. / 65535.,
                          "opaque silhouette must encode the analytic depth, not a rectangle");
                }
            }
        draw(.22f);
        for (auto value : read())
            check(value == 255, "legacy translucent blobs must not become shadow casters");
        check(shadow_map::passes == 2, "two actual submissions recorded");
        std::puts("PASS: real FBO failure, cleanup, alpha silhouette, packed depth and GL state");
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        result = 1;
    }
    shadow_map::shutdown();
    if (readback)
        glDeleteFramebuffers(1, &readback);
    if (vertices)
        glDeleteBuffers(1, &vertices);
    if (atlas)
        glDeleteTextures(1, &atlas);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
