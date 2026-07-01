#include <android_native_app_glue.h>
#include "engine.h"
#include "math_utils.h"
#include "world.h"
#include "physics.h"
#include "render.h"
#include "input.h"

static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            // Инициализация EGL здесь
            eng->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
            break;
        case APP_CMD_TERM_WINDOW:
            eng->display = EGL_NO_DISPLAY;
            break;
    }
}

static void engine_draw_frame(struct engine* eng) {
    if (!eng->display) return;
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (eng->gameState == STATE_PLAYING) {
        apply_physics(eng);
        render_world(eng);
        draw_ui(eng);
    } else {
        draw_menu(eng);
    }
    
    eglSwapBuffers(eng->display, eng->surface);
}

void android_main(struct android_app* state) {
    struct engine* eng = (struct engine*)calloc(1, sizeof(struct engine));
    eng->blocks = malloc(65 * 32 * 65);
    eng->faces = malloc(65 * 32 * 65);
    
    state->userData = eng;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;
    
    while (1) {
        int events;
        struct android_poll_source* source;
        while (ALooper_pollOnce(0, NULL, &events, (void**)&source) >= 0) {
            if (source) source->process(state, source);
            if (state->destroyRequested) return;
        }
        engine_draw_frame(eng);
    }
}
