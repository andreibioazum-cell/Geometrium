#ifndef INPUT_H
#define INPUT_H

#include <android_native_app_glue.h>
#include <math.h>
#include "engine.h"

static int32_t engine_handle_input(struct android_app* app,
                                    AInputEvent* event) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION)
        return 0;

    int action = AMotionEvent_getAction(event);
    int code   = action & AMOTION_EVENT_ACTION_MASK;
    int pCount = AMotionEvent_getPointerCount(event);

    /* DOWN / POINTER_DOWN — обрабатываем ТОЛЬКО палец который нажал */
    if (code == AMOTION_EVENT_ACTION_DOWN ||
        code == AMOTION_EVENT_ACTION_POINTER_DOWN) {

        int pi;
        if (code == AMOTION_EVENT_ACTION_DOWN) {
            pi = 0;
        } else {
            pi = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                 >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        }

        float x  = AMotionEvent_getX(event, pi);
        float y  = AMotionEvent_getY(event, pi);
        int   id = AMotionEvent_getPointerId(event, pi);

        /* Кнопка прыжка */
        float jbX = eng->width - JUMP_BTN_OFFSET;
        float jbY = eng->height - JUMP_BTN_OFFSET;
        float djx = x - jbX;
        float djy = y - jbY;

        if (sqrtf(djx * djx + djy * djy) < JUMP_BTN_SIZE * 1.5f) {
            if (eng->onGround) {
                eng->velY = 0.22f;
                eng->onGround = false;
            }
            /* НЕ назначаем этот палец ни джойстику ни камере */
            return 1;
        }

        /* Левая половина — джойстик */
        if (x < eng->width / 2) {
            eng->joyX = JOY_X_OFFSET;
            eng->joyY = eng->height - JOY_Y_OFFSET;
            eng->isMoving = true;
            eng->movePointerId = id;
            eng->moveDirX = 0;
            eng->moveDirZ = 0;
        }
        /* Правая половина — камера */
        else {
            eng->lastTouchX = x;
            eng->lastTouchY = y;
            eng->lookPointerId = id;
        }

        return 1;
    }

    /* MOVE — обрабатываем все пальцы */
    if (code == AMOTION_EVENT_ACTION_MOVE) {
        for (int i = 0; i < pCount; i++) {
            float x  = AMotionEvent_getX(event, i);
            float y  = AMotionEvent_getY(event, i);
            int   id = AMotionEvent_getPointerId(event, i);

            /* Джойстик */
            if (id == eng->movePointerId && eng->isMoving) {
                float dx = x - eng->joyX;
                float dy = y - eng->joyY;
                float d  = sqrtf(dx * dx + dy * dy);
                if (d > 10.0f) {
                    float clamp = d > JOY_RADIUS ? JOY_RADIUS : d;
                    eng->moveDirX = (dx / d) * (clamp / JOY_RADIUS);
                    eng->moveDirZ = (dy / d) * (clamp / JOY_RADIUS);
                } else {
                    eng->moveDirX = 0;
                    eng->moveDirZ = 0;
                }
            }

            /* Камера */
            if (id == eng->lookPointerId) {
                eng->camRot[1] += (x - eng->lastTouchX) * 0.004f;
                eng->camRot[0] += (y - eng->lastTouchY) * 0.004f;
                if (eng->camRot[0] >  1.4f) eng->camRot[0] =  1.4f;
                if (eng->camRot[0] < -1.4f) eng->camRot[0] = -1.4f;
                eng->lastTouchX = x;
                eng->lastTouchY = y;
            }
        }
        return 1;
    }

    /* UP / POINTER_UP — обрабатываем ТОЛЬКО палец который отпустил */
    if (code == AMOTION_EVENT_ACTION_UP ||
        code == AMOTION_EVENT_ACTION_POINTER_UP) {

        int pi = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                 >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int id = AMotionEvent_getPointerId(event, pi);

        if (id == eng->movePointerId) {
            eng->isMoving = false;
            eng->moveDirX = 0;
            eng->moveDirZ = 0;
            eng->movePointerId = -1;
        }
        if (id == eng->lookPointerId) {
            eng->lookPointerId = -1;
        }

        return 1;
    }

    return 0;
}

#endif
