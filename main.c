#include <android_native_app_glue.h>
#include <android/native_window.h>
#include <stdlib.h>
#include <string.h>  // <-- ЭТО ИСПРАВЛЯЕТ ОШИБКУ memset
#include <math.h>
#include "engine.h"

// Объявляем функции из других файлов, чтобы компилятор их видел
extern void graphics_clear(RenderBuffer* rb, uint32_t color);
extern void draw_software_block(struct engine* eng, int bx, int by, int bz, uint32_t color);
extern int32_t engine_handle_input(struct android_app* app, AInputEvent* event);

void android_main(struct android_app* s) {
    // 1. Выделяем память под движок
    struct engine* e = (struct engine*)malloc(sizeof(struct engine));
    if (!e) return;
    
    // 2. Обнуляем структуру (теперь memset будет работать)
    memset(e, 0, sizeof(struct engine));
    
    e->app = s;
    e->movePointerId = e->lookPointerId = -1;
    s->userData = e;
    s->onInputEvent = engine_handle_input;

    // 3. Генерация тестового мира
    for (int x = 0; x < WORLD_BUF; x++) {
        for (int z = 0; z < WORLD_BUF; z++) {
            e->blocks[x][5][z] = 1; // Создаем плоский мир на высоте 5
        }
    }
    
    // Начальная позиция камеры
    e->camPos[0] = 14.0f; e->camPos[1] = 8.0f; e->camPos[2] = 14.0f;

    // Главный цикл
    while (1) {
        int ev;
        struct android_poll_source* src;
        while (ALooper_pollOnce(0, 0, &ev, (void**)&src) >= 0) {
            if (src) src->process(s, src);
            if (s->destroyRequested) {
                if (e->rb.z_buffer) free(e->rb.z_buffer);
                free(e);
                return;
            }
        }

        // Если окно доступно, рисуем в него напрямую
        if (s->window) {
            ANativeWindow_Buffer win_buf;
            if (ANativeWindow_lock(s->window, &win_buf, NULL) >= 0) {
                
                // Настраиваем буфер рендера на текущее окно
                e->rb.pixels = (uint32_t*)win_buf.bits;
                e->rb.width = win_buf.width;
                e->rb.height = win_buf.height;
                e->rb.stride = win_buf.stride;

                // Создаем или пересоздаем Z-буфер под размер экрана
                if (!e->rb.z_buffer) {
                    e->rb.z_buffer = (float*)malloc(e->rb.width * e->rb.height * sizeof(float));
                }

                // Очистка экрана (Твой NEON код)
                graphics_clear(&e->rb, 0xFF88CCFF); 

                // Рендеринг блоков
                for (int x = 0; x < WORLD_BUF; x++) {
                    for (int z = 0; z < WORLD_BUF; z++) {
                        for (int y = 0; y < CHUNK_H; y++) {
                            if (e->blocks[x][y][z]) {
                                // Рисуем блок (функция в graphics.c)
                                draw_software_block(e, x, y, z, 0xFF00AA00);
                            }
                        }
                    }
                }

                ANativeWindow_unlockAndPost(s->window);
            }
        }
    }
}
