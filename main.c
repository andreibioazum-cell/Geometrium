#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "cube.h"
#include "math_utils.h"

struct engine {
    struct android_app* app;
    EGLDisplay display;
    EGLContext context;
    EGLSurface surface;
    int32_t width, height;
    GLuint program;
};

static int engine_init_display(struct engine* engine) {
    engine->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(engine->display, 0, 0);
    EGLConfig config; EGLint numConfigs;
    EGLint attribs[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE };
    eglChooseConfig(engine->display, attribs, &config, 1, &numConfigs);
    engine->surface = eglCreateWindowSurface(engine->display, config, engine->app->window, NULL);
    engine->context = eglCreateContext(engine->display, config, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
    eglMakeCurrent(engine->display, engine->surface, engine->surface, engine->context);
    eglQuerySurface(engine->display, engine->surface, EGL_WIDTH, &engine->width);
    eglQuerySurface(engine->display, engine->surface, EGL_HEIGHT, &engine->height);

    const char* vSrc = "attribute vec4 p; attribute vec2 t; varying vec2 vT; uniform mat4 m; void main() { gl_Position = m*p; vT = t; }";
    const char* fSrc = "precision mediump float; varying vec2 vT; void main() { gl_FragColor = vec4(vT.x, vT.y, 0.5, 1.0); }";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs, 1, &vSrc, NULL); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs, 1, &fSrc, NULL); glCompileShader(fs);
    engine->program = glCreateProgram();
    glAttachShader(engine->program, vs); glAttachShader(engine->program, fs);
    glLinkProgram(engine->program);
    return 0;
}

static void engine_draw_frame(struct engine* engine) {
    if (!engine->display) return;
    glClearColor(0.5f, 0.8f, 1.0f, 1.0f); // Небо
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(engine->program);

    float proj[16], view[16], model[16], rot[16], temp[16], mvp[16];
    mat4_perspective(proj, 0.8f, (float)engine->width/engine->height, 0.1f, 100.0f);
    
    static float angle = 0; angle += 0.015f;
    
    GLint mLoc = glGetUniformLocation(engine->program, "m");
    glViewport(0, 0, engine->width, engine->height);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), cube_vertices);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), &cube_vertices[3]);
    glEnableVertexAttribArray(1);

    // Рисуем сетку 5x5 кубов (как пол в Майнкрафте)
    for(int x = -2; x <= 2; x++) {
        for(int z = -2; z <= 2; z++) {
            mat4_translate(model, (float)x * 1.1f, -1.0f, (float)z * 1.1f - 7.0f);
            mat4_rotate_y(rot, angle);
            mat4_mul(temp, model, rot); // Вращаем каждый куб
            mat4_mul(mvp, proj, temp);
            glUniformMatrix4fv(mLoc, 1, GL_FALSE, mvp);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    }
    eglSwapBuffers(engine->display, engine->surface);
}

static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* engine = (struct engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) engine_init_display(engine);
}

void android_main(struct android_app* state) {
    struct engine engine = {0};
    state->userData = &engine;
    state->onAppCmd = engine_handle_cmd;
    engine.app = state;
    while (1) {
        int ident, events; struct android_poll_source* source;
        while ((ident = ALooper_pollOnce(0, NULL, &events, (void**)&source)) >= 0) {
            if (source) source->process(state, source);
            if (state->destroyRequested) return;
        }
        engine_draw_frame(&engine);
    }
}
