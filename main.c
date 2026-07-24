#include <android_native_app_glue.h>
#include <android/native_window.h>
#include <android/log.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include "engine.h"

#define LOG_TAG "HardcoreGame"

// Внешние функции
extern void graphics_clear(RenderBuffer* rb, uint32_t color);
extern void draw_block(struct engine* eng, int bx, int by, int bz, uint32_t color);

static struct engine* g_eng = NULL;

// Функция копирования в буфер через JNI (Java)
void copy_to_clipboard(struct engine* eng, const char* text) {
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "CRASH DETECTED: %s", text);
    // В реальном NDK тут нужен вызов JNI ClipboardManager, 
    // но это требует привязки к JVM. Пока выводим в лог (его легко скопировать).
}

// Обработчик сигналов (перехват вылета)
void crash_handler(int sig) {
    if (g_eng) {
        strcpy(g_eng->last_error, (sig == SIGSEGV) ? "Memory Error (SIGSEGV)" : "Math Error (SIGFPE)");
        copy_to_clipboard(g_eng, g_eng->last_error);
        longjmp(g_eng->crash_env, 1); // "Прыгаем" обратно в цикл
    }
    exit(sig);
}

void android_main(struct android_app* s) {
    struct engine* e = (struct engine*)malloc(sizeof(struct engine));
    memset(e, 0, sizeof(struct engine));
    g_eng = e;
    e->app = s;

    // Регистрация защиты от вылетов
    signal(SIGSEGV, crash_handler);
    signal(SIGFPE, crash_handler);

    // Генерация мира
    for(int x=0; x<WORLD_BUF; x++) for(int z=0; z<WORLD_BUF; z++) e->blocks[x][0][z] = 1;
    e->camPos[0]=8; e->camPos[1]=5; e->camPos[2]=8;

    while (1) {
        int ev;
        struct android_poll_source* src;
        while (ALooper_pollOnce(0, 0, &ev, (void**)&src) >= 0) {
            if (src) src->process(s, src);
            if (s->destroyRequested) return;
        }

        // --- АНАЛОГ PCALL ---
        if (setjmp(e->crash_env) == 0) {
            // Обычная работа
            if (s->window) {
                ANativeWindow_Buffer win_buf;
                if (ANativeWindow_lock(s->window, &win_buf, NULL) >= 0) {
                    e->rb.pixels = (uint32_t*)win_buf.bits;
                    e->rb.width = win_buf.width;
                    e->rb.height = win_buf.height;
                    e->rb.stride = win_buf.stride;

                    if (!e->rb.z_buffer) e->rb.z_buffer = malloc(e->rb.width * e->rb.height * sizeof(float));

                    graphics_clear(&e->rb, 0xFF88CCFF);
                    for(int x=0; x<WORLD_BUF; x++) {
                        for(int z=0; z<WORLD_BUF; z++) {
                            draw_block(e, x, 0, z, 0xFF00AA00);
                        }
                    }
                    ANativeWindow_unlockAndPost(s->window);
                }
            }
        } else {
            // Сюда попадем ТОЛЬКО при вылете
            __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "Recovered from crash!");
            // Небольшая пауза, чтобы не спамить ошибками
            usleep(1000000); 
        }
    }
}
