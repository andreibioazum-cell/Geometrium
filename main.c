#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "cube.h"
#include "math_utils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

const char* vShaderSrc = 
    "attribute vec4 pos; attribute vec2 tex; varying vec2 vTex; uniform mat4 mvp;"
    "void main() { gl_Position = mvp * pos; vTex = tex; }";
const char* fShaderSrc = 
    "precision mediump float; varying vec2 vTex; uniform sampler2D s; "
    "void main() { gl_FragColor = texture2D(s, vTex); }";

GLuint create_shader(const char* src, GLenum type) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    return s;
}

void android_main(struct android_app* state) {
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, 0, 0);
    
    EGLConfig config; EGLint numConfigs;
    EGLint attribs[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_NONE };
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);
    EGLSurface surface = eglCreateWindowSurface(display, config, state->window, NULL);
    EGLContext context = eglCreateContext(display, config, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
    eglMakeCurrent(display, surface, surface, context);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, create_shader(vShaderSrc, GL_VERTEX_SHADER));
    glAttachShader(prog, create_shader(fShaderSrc, GL_FRAGMENT_SHADER));
    glLinkProgram(prog);
    glUseProgram(prog);

    GLuint mvpLoc = glGetUniformLocation(prog, "mvp");
    float proj[16], view[16], mvp[16];
    mat4_perspective(proj, 1.0f, 1080.0f/1920.0f, 0.1f, 100.0f);

    glEnable(GL_DEPTH_TEST);
    float angle = 0;

    while (!state->destroyRequested) {
        struct android_poll_source* source;
        while (ALooper_pollAll(0, NULL, NULL, (void**)&source) >= 0) {
            if (source) source->process(state, source);
        }

        glClearColor(0.5f, 0.8f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        angle += 0.01f;
        mat4_translate(view, sinf(angle)*2.0f, -1.0f, -5.0f);
        // Тут должна быть перемножение матриц proj * view, для упрощения просто view
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, view); 

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), cube_vertices);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), &cube_vertices[3]);
        glEnableVertexAttribArray(1);

        glDrawArrays(GL_TRIANGLES, 0, 36);
        eglSwapBuffers(display, surface);
    }
}
