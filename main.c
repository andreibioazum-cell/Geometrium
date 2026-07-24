#include <android_native_app_glue.h>
#include <android/native_window.h>
#include <android/log.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include "engine.h"

// Объявляем функции из graphics.c
extern void graphics_clear(RenderBuffer* rb, uint32_t color);
extern void draw_software_block(struct engine* eng, int bx, int by, int bz, uint32_t color);

static struct engine* g_ptr = NULL;

// Обработчик сигналов (ловит Segmentation Fault)
void handle_fatal(int sig) {
    if (g_ptr) {
        __android_log_print(ANDROID_LOG_ERROR, "GEOMETRIUM", "CRASH: Signal %d. Check your pointers!", sig);
        longjmp(g_ptr->crash_env, 1);
    }
    exit(sig);
}

void android_main(struct android_app* s) {
    struct engine* e = (struct engine*)malloc(sizeof(struct engine));
    memset(e, 0, sizeof(struct engine));
    e->app = s;
    g_ptr = e;

    // Включаем перехват ошибок
    signal(SIGSEGV, handle_fatal);

    // Генерация мира (земля)
    for(int x=0; x<WORLD_BUF; x++) for(int z=0; z<WORLD_BUF; z++) e->blocks[x][0][z] = 1;
    e->camPos[0]=8; e->camPos[1]=4; e->camPos[2]=8;

    while (1) {
        int ev;
        struct android_poll_source* src;
        while (ALooper_pollOnce(0, 0, &ev, (void**)&src) >= 0) {
            if (src) src->process(s, src);
            if (s->destroyRequested) return;
        }

        // --- Эмуляция pcall (Try/Catch) ---
        if (setjmp(e->crash_env) == 0) {
            if (s->window) {
                ANativeWindow_Buffer win;
                if (ANativeWindow_lock(s->window, &win, NULL) >= 0) {
                    e->rb.pixels = (uint32_t*)win.bits;
                    e->rb.width = win.width; e->rb.height = win.height; e->rb.stride = win.stride;

                    if (!e->rb.z_buffer) e->rb.z_buffer = malloc(e->rb.width * e->rb.height * sizeof(float));

                    graphics_clear(&e->rb, 0xFF88CCFF); // Небо

                    for(int x=0; x<WORLD_BUF; x++) {
                        for(int z=0; z<WORLD_BUF; z++) {
                            draw_software_block(e, x, 0, z, 0xFF00AA00); // Трава
                        }
                    }

                    ANativeWindow_unlockAndPost(s->window);
                }
            }
        } else {
            // Если вылетело — ждем секунду и пробуем снова (не выходя из игры)
            __android_log_print(ANDROID_LOG_WARN, "GEOMETRIUM", "Recovered from error. Retrying...");
            sleep(1);
        }
    }
}
