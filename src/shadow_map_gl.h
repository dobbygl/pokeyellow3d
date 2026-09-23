#pragma once
#include "shadow_projection.h"
#include <SDL_opengles2.h>
#include <cstdio>

namespace shadow_map {
constexpr int Resolution = 1024;
inline GLuint program = 0, texture = 0, framebuffer = 0, depth = 0;
inline bool attempted = false, ready = false;
inline size_t passes = 0;
inline void shutdown() {
    if (framebuffer)
        glDeleteFramebuffers(1, &framebuffer);
    if (depth)
        glDeleteRenderbuffers(1, &depth);
    if (texture)
        glDeleteTextures(1, &texture);
    if (program)
        glDeleteProgram(program);
    program = texture = framebuffer = depth = 0;
    attempted = ready = false;
    passes = 0;
}
inline void attach(GLuint color, GLuint depth_buffer) {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_buffer);
}
// The normal allocator always attaches both resources. An alternate attachment
// operation lets GPU tests create a genuinely incomplete FBO before the real
// renderer attempts initialization, without a runtime fault switch.
template <class Compile, class Attach = decltype(&attach)>
bool initialize(Compile compile, Attach attach_resources = attach) {
    if (attempted)
        return ready;
    attempted = true;
    GLint old_framebuffer = 0, old_renderbuffer = 0, old_texture = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_framebuffer);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &old_renderbuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_texture);
    GLint range[2]{}, precision = 0;
    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, range, &precision);
    // Packed depth needs more than the optional ES2 mediump precision. A
    // device without fragment highp uses the entire reference presentation.
    bool supported = precision >= 16;
    if (supported) {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Resolution, Resolution, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
        glGenRenderbuffers(1, &depth);
        glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, Resolution, Resolution);
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        attach_resources(texture, depth);
        supported = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    }
    if (supported) {
        const char *vs = R"(
            attribute vec3 position; attribute vec2 texcoord; attribute vec4 color;
            attribute float wind; uniform mat4 view_projection; uniform float wind_time;
            varying highp vec2 uv; varying mediump float opacity;
            varying highp float shadow_depth;
            void main() {
                vec3 moved=position;
                if(wind_time>0.0 && wind>0.0) {
                    float phase=position.x*0.7+position.z*0.4;
                    moved.x+=(sin(phase+wind_time)-sin(phase))*0.065*wind;
                    moved.z+=(cos(phase+wind_time)-cos(phase))*0.035*wind;
                }
                gl_Position=view_projection*vec4(moved,1.0);
                // The light projection is orthographic (w=1). ESSL2 permits
                // mediump gl_FragCoord on some drivers, so interpolate depth
                // explicitly at highp instead of losing bits before packing.
                shadow_depth=gl_Position.z*0.5+0.5;
                uv=texcoord; opacity=color.a;
            })";
        const char *fs = R"(
            precision highp float;
            uniform sampler2D image;
            varying highp vec2 uv; varying mediump float opacity;
            varying highp float shadow_depth;
            void main() {
                // Alpha silhouettes cast; the old translucent ground blobs
                // and fully transparent sprite padding never become casters.
                if(opacity<=1.5 && texture2D(image,uv).a*opacity<0.5) discard;
                vec3 encoded=fract(min(shadow_depth,0.999999)*vec3(1.0,255.0,65025.0));
                encoded.xy-=encoded.yz/255.0;
                gl_FragColor=vec4(encoded,1.0);
            })";
        GLuint a = compile(GL_VERTEX_SHADER, vs), b = compile(GL_FRAGMENT_SHADER, fs);
        if (a && b) {
            program = glCreateProgram();
            glAttachShader(program, a);
            glAttachShader(program, b);
            glBindAttribLocation(program, 0, "position");
            glBindAttribLocation(program, 1, "texcoord");
            glBindAttribLocation(program, 2, "color");
            glBindAttribLocation(program, 3, "wind");
            glLinkProgram(program);
            GLint ok = GL_FALSE;
            glGetProgramiv(program, GL_LINK_STATUS, &ok);
            ready = ok == GL_TRUE;
        }
        if (a)
            glDeleteShader(a);
        if (b)
            glDeleteShader(b);
    }
    // Consume/report allocation errors here: a failed optional resource must
    // leave a clean GL state for the reference renderer and its frame guard.
    GLenum error;
    while ((error = glGetError()) != GL_NO_ERROR) {
        ready = false;
        std::fprintf(stderr, "[3D] shadow initialization GL error 0x%x\n", unsigned(error));
    }
    glBindFramebuffer(GL_FRAMEBUFFER, GLuint(old_framebuffer));
    glBindRenderbuffer(GL_RENDERBUFFER, GLuint(old_renderbuffer));
    glBindTexture(GL_TEXTURE_2D, GLuint(old_texture));
    if (!ready) {
        shutdown();
        attempted = true;
        std::fprintf(stderr, "[3D] Shadow target unavailable; entire artistic scene disabled.\n");
    }
    return ready;
}
template <class Draw>
void render(const sunlight::Projection &projection, float wind, GLuint atlas, Draw draw) {
    GLint target = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &target);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, Resolution, Resolution);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    const GLboolean dither = glIsEnabled(GL_DITHER);
    glDisable(GL_DITHER); // Packed byte values must not be spatially dithered.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glClearColor(1, 1, 1, 1);
    glClearDepthf(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program, "view_projection"), 1, GL_FALSE,
                       projection.matrix.data());
    glUniform1f(glGetUniformLocation(program, "wind_time"), wind);
    glUniform1i(glGetUniformLocation(program, "image"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas);
    draw();
    ++passes;
    if (dither)
        glEnable(GL_DITHER);
    glBindFramebuffer(GL_FRAMEBUFFER, GLuint(target));
}
} // namespace shadow_map
