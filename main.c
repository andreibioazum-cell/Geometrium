 #include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "engine.h"
#include "world.h"
#include "math_utils.h"
#include "physics.h"
#include "input.h"
#include "ui.h"
#include "render.h"

// Определения глобальных массивов
unsigned char map[WORLD_SIZE][CHUNK_H][WORLD_SIZE];
unsigned char face_visible[WORLD_SIZE][CHUNK_H][WORLD_SIZE];

static void engine_draw_frame(struct engine* eng) {
    if (!eng->display) return;
    apply_physics(eng);

    eglQuerySurface(eng->display, eng->surface,
                    EGL_WIDTH, &eng->width);
    eglQuerySurface(eng->display, eng->surface,
                    EGL_HEIGHT, &eng->height);

    glViewport(0, 0, eng->width, eng->height);
    glClearColor(0.5f, 0.8f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    render_world(eng);
    draw_ui(eng);

    eglSwapBuffers(eng->display, eng->surface);
}

static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) {
        eng->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(eng->display, 0, 0);

        EGLConfig config; EGLint n;
        EGLint att[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_DEPTH_SIZE, 16,
            EGL_NONE
        };
        eglChooseConfig(eng->display, att, &config, 1, &n);
        eng->surface = eglCreateWindowSurface(
            eng->display, config, eng->app->window, NULL);
        EGLint ctxAtt[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
        eng->context = eglCreateContext(
            eng->display, config, NULL, ctxAtt);
        eglMakeCurrent(eng->display, eng->surface,
                       eng->surface, eng->context);

        // Шейдер мира
        const char* vS =
            "attribute vec4 p; attribute vec2 t;"
            "varying vec2 vT; uniform mat4 m;"
            "void main(){ gl_Position=m*p; vT=t; }";
        const char* fS =
            "precision mediump float; varying vec2 vT;"
            "void main(){"
            "  gl_FragColor=vec4(vT.x, vT.y+0.3, 0.4, 1.0);"
            "}";
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vS, NULL); glCompileShader(vs);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fS, NULL); glCompileShader(fs);
        eng->program = glCreateProgram();
        glAttachShader(eng->program, vs);
        glAttachShader(eng->program, fs);
        glLinkProgram(eng->program);
        glDeleteShader(vs);
        glDeleteShader(fs);

        init_ui_shader();
        rebuild_world_vbo(eng);
    }
}

void android_main(struct android_app* state) {
    struct engine eng = {0};
    eng.movePointerId = -1;
    eng.lookPointerId = -1;

    generate_world();
    eng.camPos[0] = WORLD_SIZE / 2.0f;
    eng.camPos[2] = -(WORLD_SIZE / 2.0f);
    eng.camPos[1] = get_spawn_y((int)eng.camPos[0],
                                 (int)(-eng.camPos[2]));

    state->userData = &eng;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;
    eng.app = state;

    while (1) {
        int ev;
        struct android_poll_source* s;
        while (ALooper_pollOnce(0, NULL, &ev, (void**)&s) >= 0) {
            if (s) s->process(state, s);
            if (state->destroyRequested) return;
        }
        engine_draw_frame(&eng);
    }
}
