#pragma once
#include "presentation_blend.h"

// Included after shader() in pallet3d.cpp. Each texture is an entire completed
// presentation, including the original LCD menus and the prototype's HUD.
namespace presentation {
inline Blend blend;
struct Frame {
    GLuint texture = 0;
    int width = 0, height = 0;
    bool valid = false;
};
inline Frame history, source;
inline GLuint program = 0, buffer = 0;
inline void shutdown() {
    if (history.texture)
        glDeleteTextures(1, &history.texture);
    if (source.texture)
        glDeleteTextures(1, &source.texture);
    if (program)
        glDeleteProgram(program);
    if (buffer)
        glDeleteBuffers(1, &buffer);
    history = {};
    source = {};
    program = buffer = 0;
    blend = {};
}
inline bool initialize() {
    if (program)
        return true;
    const char *vs = "attribute vec2 position;varying vec2 uv;void "
                     "main(){gl_Position=vec4(position,0.0,1.0);uv=position*0.5+0.5;}";
    const char *fs = "precision mediump float;uniform sampler2D image;uniform float alpha;varying "
                     "vec2 uv;void main(){gl_FragColor=vec4(texture2D(image,uv).rgb,alpha);}";
    GLuint a = shader(GL_VERTEX_SHADER, vs), b = shader(GL_FRAGMENT_SHADER, fs);
    if (!a || !b) {
        if (a)
            glDeleteShader(a);
        if (b)
            glDeleteShader(b);
        return false;
    }
    program = glCreateProgram();
    glAttachShader(program, a);
    glAttachShader(program, b);
    glBindAttribLocation(program, 0, "position");
    glLinkProgram(program);
    glDeleteShader(a);
    glDeleteShader(b);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        shutdown();
        return false;
    }
    const float points[]{-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}
inline bool draw(const Frame &frame, int w, int h, float alpha = 1) {
    if (!frame.valid || !program)
        return false;
    glViewport(0, 0, w, h);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, frame.texture);
    glUniform1i(glGetUniformLocation(program, "image"), 0);
    glUniform1f(glGetUniformLocation(program, "alpha"), alpha);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    return true;
}
inline void capture(Frame &frame, int w, int h) {
    if (w <= 0 || h <= 0 || !initialize())
        return;
    if (!frame.texture) {
        glGenTextures(1, &frame.texture);
        glBindTexture(GL_TEXTURE_2D, frame.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, frame.texture);
    if (frame.width != w || frame.height != h) {
        frame.width = w;
        frame.height = h;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
    glBindTexture(GL_TEXTURE_2D, 0);
    frame.valid = true;
}
inline void begin(bool desired, bool frozen, uint32_t now) {
    if (blend.begin(desired, frozen, now, history.valid))
        std::swap(history, source);
}
inline void finish(int w, int h, bool frozen) {
    // Settings must stay crisp and must never become the source of a game fade.
    if (frozen)
        return;
    if (blend.running)
        draw(source, w, h, 1 - blend.progress());
    capture(history, w, h);
}
} // namespace presentation
