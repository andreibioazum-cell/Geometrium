#include "engine.h"
#include <arm_neon.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

// Очистка экрана (Твой NEON код)
void graphics_clear(RenderBuffer* rb, uint32_t color) {
    if (!rb->pixels) return;
    uint32x4_t v_color = vdupq_n_u32(color);
    int total = rb->stride * rb->height;
    int i = 0;
    for (; i <= total - 4; i += 4) {
        vst1q_u32(&rb->pixels[i], v_color);
    }
    for (; i < total; i++) rb->pixels[i] = color;
    
    // Очистка Z-буфера
    if (rb->z_buffer) {
        for (int j = 0; j < rb->width * rb->height; j++) rb->z_buffer[j] = 1000.0f;
    }
}

// Математика проекции блока на экран
void project_block(struct engine* eng, float x, float y, float z, int* sx, int* sy, float* sz) {
    float dx = x - eng->camPos[0];
    float dy = y - eng->camPos[1];
    float dz = z - eng->camPos[2];

    float cosY = cosf(eng->camRot[1]), sinY = sinf(eng->camRot[1]);
    float tx = dx * cosY - dz * sinY;
    float tz = dx * sinY + dz * cosY;

    *sz = tz;
    if (tz > 0.1f) {
        float fov = eng->rb.height;
        *sx = (int)(eng->rb.width / 2 + (tx / tz) * fov);
        *sy = (int)(eng->rb.height / 2 - (dy / tz) * fov);
    } else {
        *sx = -10000; *sy = -10000;
    }
}

// Рисование блока (софтверное)
void draw_software_block(struct engine* eng, int bx, int by, int bz, uint32_t color) {
    int sx, sy; float sz;
    project_block(eng, (float)bx, (float)by, (float)bz, &sx, &sy, &sz);

    if (sz < 0.1f || sz > 100.0f) return;

    int size = (int)(eng->rb.height / (sz * 2)); // Размер блока
    if (size <= 0 || size > 500) return;

    int x1 = sx - size/2, x2 = sx + size/2;
    int y1 = sy - size/2, y2 = sy + size/2;

    // Проверка границ экрана (чтобы не было вылета)
    if (x1 < 0) x1 = 0; if (x2 >= eng->rb.width) x2 = eng->rb.width - 1;
    if (y1 < 0) y1 = 0; if (y2 >= eng->rb.height) y2 = eng->rb.height - 1;

    for (int y = y1; y <= y2; y++) {
        uint32_t* row = eng->rb.pixels + (y * eng->rb.stride);
        float* z_row = eng->rb.z_buffer + (y * eng->rb.width);
        for (int x = x1; x <= x2; x++) {
            if (sz < z_row[x]) {
                row[x] = color;
                z_row[x] = sz;
            }
        }
    }
}
