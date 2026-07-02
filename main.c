#include "engine.h"
#include "render.h"
#include "physics.h"
#include "input.h"

unsigned int game_seed = 12345;

void android_main(struct android_app* state) {
    struct engine eng = {0};
    state->userData = &eng;
    state->onAppCmd = NULL; // Здесь должна быть инициализация EGL из твоего main.c
    state->onInputEvent = engine_handle_input;
    eng.app = state;
    eng.camPos[1] = 10.0f;

    while (1) {
        int ev; struct android_poll_source* s;
        while (ALooper_pollOnce(0, NULL, &ev, (void**)&s) >= 0) {
            if (s) s->process(state, s);
            if (state->destroyRequested) return;
        }
        if (eng.display) {
            update_world(&eng);
            apply_physics(&eng);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            // Здесь вызовы рендеринга
            eglSwapBuffers(eng.display, eng.surface);
        }
    }
}
