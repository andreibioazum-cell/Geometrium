#ifndef ENGINE_H
#define ENGINE_H

#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define PI 3.14159265f
#define WORLD_BUF 65
#define CHUNK_H 32
#define LOAD_RADIUS 32
#define MAX_EDITS 1024
#define RAY_DIST 6.0f
#define RAY_STEP 0.05f

#define STATE_MENU 0
#define STATE_PLAYING 1

#define FACE_XP 0x01
#define FACE_XN 0x02
#define FACE_YP 0x04
#define FACE_YN 0x08
#define FACE_ZP 0x10
#define FACE_ZN 0x20

struct block_edit { int wx, wy, wz; unsigned char val; };

struct engine {
    struct android_app* app;
    EGLDisplay display; EGLSurface surface; EGLContext context;
    int32_t width, height;
    GLuint program;
    float camPos[3], camRot[2], velY;
    bool onGround, isMoving, worldLoaded, meshDirty;
    unsigned char (*blocks)[32][65];
    unsigned char (*faces)[32][65];
    int loadCenterX, loadCenterZ, editCount, visibleFaceCount, gameState;
    unsigned int worldSeed;
    int seedDigits[6], seedCursor;
    struct block_edit edits[MAX_EDITS];
    // Поля для анимации
    bool animActive; int animTimer; float animBlockPos[3];
};

#endif
