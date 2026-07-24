#ifndef ENGINE_H
#define ENGINE_H

#include <android_native_app_glue.h>
#include <stdint.h>
#include <stdbool.h>

// Константы из твоего проекта
#define WORLD_BUF 29
#define CHUNK_H 32
#define JOY_RADIUS 80.0f
#define JOY_X_OFFSET 130.0f

typedef struct {
    uint32_t* pixels; // Массив пикселей экрана
    float* z_buffer;  // Буфер глубины (чтобы блоки не просвечивали)
    int width, height, stride;
} RenderBuffer;

struct engine {
    struct android_app* app;
    RenderBuffer rb; // Наш самодельный "экран"

    float camPos[3], camRot[2];
    float moveDirX, moveDirZ;
    bool isMoving;

    // Твой массив блоков
    unsigned char blocks[WORLD_BUF][CHUNK_H][WORLD_BUF];
    int loadCenterX, loadCenterZ;
    
    int movePointerId, lookPointerId;
    float lastTouchX, lastTouchY;
};

#endif
