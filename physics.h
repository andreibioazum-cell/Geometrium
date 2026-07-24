#include "engine.h"
#include <arm_neon.h>
#include <math.h>
#include <stdlib.h>

// Твоя функция очистки экрана на NEON
void graphics_clear(RenderBuffer* rb, uint32_t color) {
    uint32x4_t v_color = vdupq_n_u32(color);
    int total_pixels = rb->stride * rb->height;
    int i = 0;
    for (; i <= total_pixels - 4; i += 4) {
        vst1q_u32(&rb->pixels[i], v_color);
    }
    for (; i < total_pixels; i++) rb->pixels[i] = color;
    
    // Очистка Z-буфера
    for (int j = 0; j < total_pixels; j++) rb->z_buffer[j] = 1000.0f;
}

// Математика проекции 3D -> 2D (вместо шейдеров)
void project_block(struct engine* eng, float x, float y, float z, int* outX, int* outY, float* outZ) {
    float dx = x - eng->camPos[0];
    float dy = y - eng->camPos[1];
    float dz = z - eng->camPos[2];

    // Поворот камеры
    float cosY = cosf(eng->camRot[1]), sinY = sinf(eng->camRot[1]);
    float tx = dx * cosY - dz * sinY;
    float tz = dx * sinY + dz * cosY;

    *outZ = tz;
    if (tz > 0.1f) {
        float fov = eng->rb.height;
        *outX = (int)(eng->rb.width / 2 + (tx / tz) * fov);
        *outY = (int)(eng->rb.height / 2 - (dy / tz) * fov);
    } else {
        *outX = -1000; *outY = -1000;
    }
}

// Рисование одного блока (пикселями)
void draw_software_block(struct engine* eng, int bx, int by, int bz, uint32_t color) {
    int sx, sy; float sz;
    project_block(eng, (float)bx, (float)by, (float)bz, &sx, &sy, &sz);

    if (sz < 0.1f) return;

    int size = (int)(eng->rb.height / sz); // Размер кубика на экране
    if (size <= 0) return;

    for (int y = sy - size/2; y < sy + size/2; y++) {
        if (y < 0 || y >= eng->rb.height) continue;
        for (int x = sx - size/2; x < sx + size/2; x++) {
            if (x < 0 || x >= eng->rb.width) continue;
            
            int idx = y * eng->rb.stride + x;
            if (sz < eng->rb.z_buffer[idx]) {
                eng->rb.pixels[idx] = color;
                eng->rb.z_buffer[idx] = sz;
            }
        }
    }
}
