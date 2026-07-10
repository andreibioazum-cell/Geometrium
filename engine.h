#ifndef ENGINE_H
#define ENGINE_H

#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdbool.h>

#define JOY_RADIUS      80.0f
#define JOY_X_OFFSET    130.0f
#define JOY_Y_OFFSET    130.0f
#define STICK_RADIUS    32.0f

#define JUMP_BTN_SIZE   80.0f
#define JUMP_BTN_OFFSET 130.0f

#define ACTION_BTN_SIZE 45.0f
#define BREAK_BTN_X     80.0f
#define BREAK_BTN_Y     80.0f
#define PLACE_BTN_X     80.0f
#define PLACE_BTN_Y     190.0f

#define PI              3.14159265f

#define PLAYER_W        0.4f
#define EYE_H           1.65f
#define HEAD_MARGIN     0.15f
#define GAME_FOV        1.4915f

#define GRAVITY         0.005f
#define JUMP_FORCE      0.12f
#define TERM_VEL       -0.25f

#define LOAD_RADIUS     32
#define WORLD_BUF       (LOAD_RADIUS * 2 + 1)
#define CHUNK_H         32

#define FACE_XP 0x01
#define FACE_XN 0x02
#define FACE_YP 0x04
#define FACE_YN 0x08
#define FACE_ZP 0x10
#define FACE_ZN 0x20

#define RAY_DIST        6.0f
#define RAY_STEP        0.02f
#define MAX_EDITS       512

#define BLOCK_AIR       0
#define BLOCK_GRASS     1

#define INV_SLOTS       9
#define INV_SLOT_SIZE   46.0f
#define INV_PADDING     4.0f
#define INV_Y_OFFSET    50.0f

#define ANIM_BREAK_FRAMES 12
#define ANIM_PLACE_FRAMES 10

#define STATE_MENU      0
#define STATE_PLAYING   1

struct block_edit {
    int wx, wy, wz;
    unsigned char val;
};

struct engine {
    struct android_app* app;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width, height;
    GLuint program;
    GLuint invProgram;
    GLuint texGrassTop;
    GLuint texGrassSide;
    GLuint texGrassDown;

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

    int loadCenterX, loadCenterZ;
    bool worldLoaded;

    unsigned char blocks[WORLD_BUF][CHUNK_H][WORLD_BUF];
    unsigned char faces[WORLD_BUF][CHUNK_H][WORLD_BUF];

    struct block_edit edits[MAX_EDITS];
    int editCount;

    unsigned char invSlots[INV_SLOTS];
    int selectedSlot;

    int animBreakTimer;
    int animPlaceTimer;
    float animBlockX, animBlockY, animBlockZ;
    bool animActive;
    bool animIsBreak;

    int gameState;
    int worldSeed;
    int seedDigits[6];
    int seedCursor;
    
    bool joyTouched;
};

#endif
