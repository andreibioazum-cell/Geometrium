#include <android_native_app_glue.h>
#include <stdlib.h>
#include "engine.h"
#include "world.h"
#include "input.h"
#include "render.h"

static void engine_draw_frame(struct engine* eng) {
    if (!eng->display) return;
    
    if (eng->gameState == STATE_PLAYING) {
        if (!eng->worldLoaded) {
            int px = (int)floorf(eng->camPos[0]);
            int pz = (int)floorf(-eng->camPos[2]);
            load_blocks_around(eng, px, pz);
        }
        apply_physics(eng);
    }

    glClearColor(0.53f, 0.81f, 0.98f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (eng->gameState == STATE_PLAYING) {
        render_world(eng);
        draw_ui(eng);
    } else {
        draw_menu(eng);
    }
    
    eglSwapBuffers(eng->display, eng->surface);
}

void android_main(struct android_app* state) {
    // Выделяем engine в куче
    struct engine* eng = (struct engine*)calloc(1, sizeof(struct engine));
    if (!eng) return;

    // Выделяем массивы мира (по 270 КБ каждый)
    eng->blocks = malloc(WORLD_BUF * CHUNK_H * WORLD_BUF);
    eng->faces  = malloc(WORLD_BUF * CHUNK_H * WORLD_BUF);
    
    eng->app = state;
    state->userData = eng;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;

    // Инициализация инвентаря
    eng->invSlots[0] = BLOCK_GRASS;
    eng->invSlots[1] = BLOCK_DIRT;
    eng->invSlots[2] = BLOCK_STONE;
    eng->selectedSlot = 0;
    eng->gameState = STATE_MENU;

    while (1) {
        int events;
        struct android_poll_source* source;
        while (ALooper_pollOnce(0, NULL, &events, (void**)&source) >= 0) {
            if (source) source->process(state, source);
            if (state->destroyRequested) {
                free(eng->blocks);
                free(eng->faces);
                free(eng);
                return;
            }
        }
        engine_draw_frame(eng);
    }
}
