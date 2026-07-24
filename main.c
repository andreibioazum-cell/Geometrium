#include "engine.h"
#include <android/native_window.h>
#include <malloc.h>

// Внешние функции из graphics.c
void graphics_clear(RenderBuffer* rb, uint32_t color);
void draw_software_block(struct engine* eng, int bx, int by, int bz, uint32_t color);

void android_main(struct android_app* app) {
    struct engine* eng = (struct engine*)malloc(sizeof(struct engine));
    memset(eng, 0, sizeof(struct engine));
    app->userData = eng;

    // Генерация мира (как в твоем world.h)
    for (int x = 0; x < WORLD_BUF; x++) {
        for (int z = 0; z < WORLD_BUF; z++) {
            eng->blocks[x][5][z] = 1; // Слой земли
        }
    }
    eng->camPos[0] = 15; eng->camPos[1] = 7; eng->camPos[2] = 5;

    while (1) {
        int events;
        struct android_poll_source* source;
        while (ALooper_pollOnce(0, 0, &events, (void**)&source) >= 0) {
            if (source) source->process(app, source);
            if (app->destroyRequested) return;
        }

        if (app->window) {
            ANativeWindow_Buffer win_buffer;
            // Блокируем окно для прямого доступа к пикселям
            if (ANativeWindow_lock(app->window, &win_buffer, NULL) >= 0) {
                
                eng->rb.pixels = (uint32_t*)win_buffer.bits;
                eng->rb.width = win_buffer.width;
                eng->rb.height = win_buffer.height;
                eng->rb.stride = win_buffer.stride;

                // Создаем Z-буфер если его нет
                if (!eng->rb.z_buffer) {
                    eng->rb.z_buffer = malloc(eng->rb.width * eng->rb.height * sizeof(float));
                }

                // 1. Очистка экрана (Твой NEON)
                graphics_clear(&eng->rb, 0xFF88CCFF); // Голубое небо

                // 2. Рисуем мир пикселями
                for (int x = 0; x < WORLD_BUF; x++) {
                    for (int z = 0; z < WORLD_BUF; z++) {
                        for (int y = 0; y < CHUNK_H; y++) {
                            if (eng->blocks[x][y][z]) {
                                uint32_t col = (y > 5) ? 0xFF00FF00 : 0xFF777777;
                                draw_software_block(eng, x, y, z, col);
                            }
                        }
                    }
                }

                // Выводим все на экран
                ANativeWindow_unlockAndPost(app->window);
            }
        }
    }
}
