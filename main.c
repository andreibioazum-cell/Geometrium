#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "engine.h"
#include "world.h"
#include "math_utils.h"
#include "physics.h"
#include "input.h"
#include "render.h"

static void engine_draw_frame(struct engine* eng) {
    if (!eng->display) return;

    update_world(eng);
    apply_physics(eng);

    eglQuerySurface(eng->display, eng->surface, EGL_WIDTH, &eng->width);
    eglQuerySurface(eng->display, eng->surface, EGL_HEIGHT, &eng->height);

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

        EGLConfig config;
        EGLint n;
        EGLint att[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_DEPTH_SIZE, 16,
            EGL_NONE
        };
        eglChooseConfig(eng->display, att, &config, 1, &n);
        eng->surface = eglCreateWindowSurface(
            eng->display, config, eng->app->window, NULL);
        EGLint ctxAtt[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        eng->context = eglCreateContext(
            eng->display, config, NULL, ctxAtt);
        eglMakeCurrent(eng->display, eng->surface,
                       eng->surface, eng->context);

        /* Шейдер мира — с текстурой */
        const char* vS =
            "attribute vec4 p; attribute vec2 t;"
            "varying vec2 vT; uniform mat4 m;"
            "void main(){ gl_Position=m*p; vT=t; }";
        const char* fS =
            "precision mediump float;"
            "varying vec2 vT;"
            "uniform sampler2D tex;"
            "void main(){"
            "  gl_FragColor = texture2D(tex, vT);"
            "}";

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vS, NULL);
        glCompileShader(vs);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fS, NULL);
        glCompileShader(fs);
        eng->program = glCreateProgram();
        glAttachShader(eng->program, vs);
        glAttachShader(eng->program, fs);
        glLinkProgram(eng->program);
        glDeleteShader(vs);
        glDeleteShader(fs);

        /* Загрузка текстуры травы */
        eng->texture = load_texture(eng->app, "grass.png");

        init_ui_shader();
    }
}

void android_main(struct android_app* state) {
    struct engine eng = {0};
    eng.movePointerId = -1;
    eng.lookPointerId = -1;
    eng.worldLoaded = false;
    eng.meshDirty = true;

    eng.camPos[0] = 0.5f;
    eng.camPos[2] = -0.5f;
    eng.camPos[1] = (float)get_height(0, 0) + 2.5f;

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
