#pragma once

// Included after shader() in pallet3d.cpp. The retained world is still rendered
// from its resident meshes. This texture is a temporary postprocess source;
// it never replaces the scene's geometry or reads game memory.
namespace scene_filter {
inline GLuint program = 0, texture = 0, buffer = 0;
inline int width = 0, height = 0;
inline bool valid = false;
inline void invalidate() {
    valid = false;
}
inline void shutdown() {
    if (program)
        glDeleteProgram(program);
    if (texture)
        glDeleteTextures(1, &texture);
    if (buffer)
        glDeleteBuffers(1, &buffer);
    program = texture = buffer = 0;
    width = height = 0;
    valid = false;
}
inline bool initialize() {
    if (program)
        return true;
    const char *vs = R"(
        attribute vec2 position; varying vec2 uv;
        void main(){gl_Position=vec4(position,0.0,1.0);uv=position*0.5+0.5;}
    )";
    const char *fs = R"(
        precision mediump float;
        uniform sampler2D image; uniform vec2 step_size; uniform float dim;
        uniform float radial; uniform vec2 radial_center;
        varying vec2 uv;
        void main() {
            if(radial>0.0) {
                vec3 c=vec3(0.0);vec2 ray=(radial_center-uv)*radial;
                for(int i=0;i<8;i++)c+=texture2D(image,uv+ray*(float(i)/7.0)).rgb;
                gl_FragColor=vec4(c*(dim/8.0),1.0);return;
            }
            vec3 c=texture2D(image,uv).rgb*4.0;
            c+=(texture2D(image,uv+vec2(step_size.x,0.0)).rgb+
                texture2D(image,uv-vec2(step_size.x,0.0)).rgb+
                texture2D(image,uv+vec2(0.0,step_size.y)).rgb+
                texture2D(image,uv-vec2(0.0,step_size.y)).rgb)*2.0;
            c+=texture2D(image,uv+step_size).rgb+texture2D(image,uv-step_size).rgb+
                texture2D(image,uv+vec2(step_size.x,-step_size.y)).rgb+
                texture2D(image,uv+vec2(-step_size.x,step_size.y)).rgb;
            gl_FragColor=vec4(c*(dim/16.0),1.0);
        }
    )";
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
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    const float vertices[]{-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}
inline bool capture(int w, int h) {
    if (!initialize())
        return false;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    if (width != w || height != h) {
        width = w;
        height = h;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
    glBindTexture(GL_TEXTURE_2D, 0);
    valid = true;
    return true;
}
inline bool draw(int w, int h, float dim = .42f, float radius = 4.f, float radial = 0,
                 float center_x = .5f, float center_y = .5f) {
    if (!valid || !program || width != w || height != h)
        return false;
    glViewport(0, 0, w, h);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(program, "image"), 0);
    glUniform2f(glGetUniformLocation(program, "step_size"), radius / w, radius / h);
    glUniform1f(glGetUniformLocation(program, "dim"), dim);
    glUniform1f(glGetUniformLocation(program, "radial"), radial);
    glUniform2f(glGetUniformLocation(program, "radial_center"), center_x, center_y);
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
} // namespace scene_filter
