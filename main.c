#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdbool.h>
#include <math.h>
#include "cube.h"
#include "math_utils.h"
#include "world.h"

struct engine {
    struct android_app* app;
    EGLDisplay display; EGLSurface surface; EGLContext context;
    int32_t width, height;
    GLuint program;

    float camPos[3], camRot[2];
    float velY;
    float joyStartX, joyStartY, moveDirX, moveDirZ;
    float lastTouchX, lastTouchY;
    bool isMoving, isLooking;
};

// Исправленная проверка коллизий
bool is_solid(float x, float y, float z) {
    int ix = (int)floorf(x + 0.5f);
    int iy = (int)floorf(y);
    int iz = (int)floorf(-z + 0.5f); // Z инвертирован в отрисовке

    if (ix < 0 || ix >= WORLD_SIZE || iz < 0 || iz >= WORLD_SIZE || iy < 0 || iy >= CHUNK_H) 
        return false;
    return map[ix][iy][iz] > 0;
}

static void apply_physics(struct engine* eng) {
    // 1. Гравитация
    eng->velY -= 0.012f;
    float nextY = eng->camPos[1] + eng->velY;

    // Проверка пола (под ногами)
    if (is_solid(eng->camPos[0], nextY - 1.6f, eng->camPos[2])) {
        eng->velY = 0;
    } else {
        eng->camPos[1] = nextY;
    }

    // 2. Движение
    if (eng->isMoving) {
        float speed = 0.1f;
        float yaw = eng->camRot[1];
        float dx = (sinf(yaw) * -eng->moveDirZ + cosf(yaw) * eng->moveDirX) * speed;
        float dz = (-cosf(yaw) * -eng->moveDirZ + sinf(yaw) * eng->moveDirX) * speed;

        // Коллизии со стенами (проверяем на уровне тела)
        if (!is_solid(eng->camPos[0] + dx, eng->camPos[1] - 0.5f, eng->camPos[2] + dz)) {
            eng->camPos[0] += dx;
            eng->camPos[2] += dz;
        }
    }

    // 3. Авто-респавн
    if (eng->camPos[1] < -2.0f) {
        eng->camPos[0] = WORLD_SIZE / 2;
        eng->camPos[2] = -(WORLD_SIZE / 2);
        eng->camPos[1] = get_spawn_y((int)eng->camPos[0], (int)-eng->camPos[2]);
        eng->velY = 0;
    }
}

static int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        int action = AMotionEvent_getAction(event);
        int pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        float x = AMotionEvent_getX(event, pointerIndex);
        float y = AMotionEvent_getY(event, pointerIndex);
        int code = action & AMOTION_EVENT_ACTION_MASK;

        if (x < eng->width / 2) { // Лево - Ходьба
            if (code == AMOTION_EVENT_ACTION_DOWN || code == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                eng->joyStartX = x; eng->joyStartY = y; eng->isMoving = true;
            } else if (code == AMOTION_EVENT_ACTION_MOVE) {
                float dx = x - eng->joyStartX, dy = y - eng->joyStartY;
                float d = sqrtf(dx*dx + dy*dy);
                if (d > 5.0f) { eng->moveDirX = dx/d; eng->moveDirZ = dy/d; }
            } else if (code == AMOTION_EVENT_ACTION_UP || code == AMOTION_EVENT_ACTION_POINTER_UP) {
                eng->isMoving = false;
            }
        } else { // Право - Обзор + Прыжок
            if (code == AMOTION_EVENT_ACTION_DOWN || code == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                eng->lastTouchX = x; eng->lastTouchY = y;
                if (eng->velY == 0) eng->velY = 0.22f; // Прыжок
            } else if (code == AMOTION_EVENT_ACTION_MOVE) {
                eng->camRot[1] += (x - eng->lastTouchX) * 0.005f;
                eng->camRot[0] += (y - eng->lastTouchY) * 0.005f;
                eng->lastTouchX = x; eng->lastTouchY = y;
            }
        }
        return 1;
    }
    return 0;
}

// ... Остальные функции (draw_frame, engine_handle_cmd) остаются прежними ...
// Но в draw_frame убедись, что используется:
// mat4_translate(model, (float)x, (float)y, (float)-z);

void android_main(struct android_app* state) {
    struct engine eng = {0};
    generate_world();
    
    // Устанавливаем игрока в центр мира на поверхность
    eng.camPos[0] = WORLD_SIZE / 2;
    eng.camPos[2] = -(WORLD_SIZE / 2);
    eng.camPos[1] = get_spawn_y((int)eng.camPos[0], (int)-eng.camPos[2]);

    state->userData = &eng;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;
    eng.app = state;

    while (1) {
        int id, ev; struct android_poll_source* s;
        while ((id = ALooper_pollOnce(0, NULL, &ev, (void**)&s)) >= 0) {
            if (s) s->process(state, s);
            if (state->destroyRequested) return;
        }
        engine_draw_frame(&eng);
    }
}
