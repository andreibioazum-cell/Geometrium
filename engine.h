#ifndef ENGINE_H
#define ENGINE_H

#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdbool.h>

#define JOY_RADIUS      110.0f
#define JOY_Y_OFFSET    200.0f
#define JOY_X_OFFSET    180.0f
#define STICK_RADIUS    44.0f

#define JUMP_BTN_SIZE   110.0f
#define JUMP_BTN_OFFSET 180.0f

#define PI              3.14159265f

#define PLAYER_W        0.4f
#define PLAYER_H        2.0f
#define EYE_H           1.65f
#define HEAD_MARGIN     0.15f
#define GAME_FOV        1.4915f

#define GRAVITY         0.005f
#define JUMP_FORCE      0.12f
#define TERM_VEL       -0.25f

/* Радиус загрузки блоков вокруг игрока */
#define LOAD_RADIUS     40
#define WORLD_BUF       (LOAD_RADIUS * 2 + 1)
#define CHUNK_H         10

#define FACE_XP 0x01
#define FACE_XN 0x02
#define FACE_YP 0x04
#define FACE_YN 0x08
#define FACE_ZP 0x10
#define FACE_ZN 0x20

struct engine {
    struct android_app* app;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width, height;
    GLuint program;
    GLuint texture;

    float camPos[3];
    float camRot[2];
    float velY;

    float joyX, joyY;
    float moveDirX, moveDirZ;
    float lastTouchX, lastTouchY;

    bool isMoving;
    int movePointerId;
    int lookPointerId;
    bool onGround;

    GLuint vbo;
    int visibleFaceCount;
    bool meshDirty;

    /* Центр загруженной области в мировых координатах */
    int loadCenterX, loadCenterZ;
    bool worldLoaded;

    /* Блоки вокруг игрока */
    unsigned char blocks[WORLD_BUF][CHUNK_H][WORLD_BUF];
    unsigned char faces[WORLD_BUF][CHUNK_H][WORLD_BUF];
};

#endif
