#ifndef ENGINE_H
#define ENGINE_H

#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdbool.h>

#define JOY_RADIUS      70.0f
#define JOY_Y_OFFSET    160.0f
#define JOY_X_OFFSET    140.0f
#define STICK_RADIUS    28.0f
#define JUMP_BTN_SIZE   55.0f
#define JUMP_BTN_OFFSET 130.0f
#define PI              3.14159265f

#define PLAYER_W        0.4f
#define PLAYER_H        2.0f
#define EYE_H           1.65f
#define HEAD_MARGIN     0.15f
#define GAME_FOV        1.4915f

struct engine {
    struct android_app* app;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width, height;
    GLuint program;

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
};

#endif
