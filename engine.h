#ifndef ENGINE_H
#define ENGINE_H

#include <android_native_app_glue.h>
#include <stdint.h>
#include <stdbool.h>

#define PI 3.14159265f
#define JOY_RADIUS 80.0f
#define JOY_X_OFFSET 130.0f
#define STICK_RADIUS 32.0f
#define JUMP_BTN_SIZE 80.0f
#define JUMP_BTN_OFFSET 130.0f

#define WORLD_BUF 29
#define CHUNK_H 32
#define PLAYER_W 0.4f
#define EYE_H 1.65f

// Типы блоков
#define BLOCK_AIR 0
#define BLOCK_GRASS 1
#define BLOCK_WOOD 2
#define BLOCK_LEAVES 3

typedef struct {
    uint32_t* pixels;
    float* z_buffer;
    int width, height, stride;
} RenderBuffer;

struct engine {
    struct android_app* app;
    RenderBuffer rb;
    
    float camPos[3], camRot[2], velY;
    float moveDirX, moveDirZ;
    bool isMoving, onGround;
    int movePointerId, lookPointerId;
    float lastTouchX, lastTouchY;

    unsigned char blocks[WORLD_BUF][CHUNK_H][WORLD_BUF];
    int loadCenterX, loadCenterZ;
    bool worldLoaded;
    
    int selectedSlot;
    uint32_t* textures[6]; // Текстуры в виде массивов пикселей
    int texW, texH;
};

#endif
