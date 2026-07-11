#ifndef INPUT_H
#define INPUT_H

#include <android_native_app_glue.h>
#include <math.h>
#include "engine.h"
#include "world.h"

static int32_t handle_menu_input(struct engine* eng, float x, float y) {
    int sw = eng->width, sh = eng->height;
    // Кнопка Play (Центральный белый прямоугольник 200x80)
    float bx = sw / 2.0f, by = sh / 2.0f;
    if (x > bx - 100 && x < bx + 100 && y > by - 40 && y < by + 40) {
        eng->gameState = STATE_PLAYING;
        eng->worldLoaded = false;
        eng->editCount = 0;
        eng->camPos[0] = 0.5f;
        eng->camPos[2] = -0.5f;
        eng->camPos[1] = 20.0f;
        eng->velY = 0;
        return 1;
    }
    return 0;
}

static int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;

    int action = AMotionEvent_getAction(event);
    int code = action & AMOTION_EVENT_ACTION_MASK;
    int pCount = AMotionEvent_getPointerCount(event);

    if (code == AMOTION_EVENT_ACTION_DOWN || code == AMOTION_EVENT_ACTION_POINTER_DOWN) {
        int pi = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        float x = AMotionEvent_getX(event, pi);
        float y = AMotionEvent_getY(event, pi);
        int id = AMotionEvent_getPointerId(event, pi);

        if (eng->gameState == STATE_MENU) return handle_menu_input(eng, x, y);

        // --- ИНВЕНТАРЬ ---
        float invW = INV_SLOTS * 50.0f;
        float invStartX = (eng->width - invW) / 2.0f;
        if (y > eng->height - 80 && x > invStartX && x < invStartX + invW) {
            int slot = (int)((x - invStartX) / 50.0f);
            if (slot >= 0 && slot < INV_SLOTS) { eng->selectedSlot = slot; return 1; }
        }

        // --- ПРЫЖОК ---
        float jbX = eng->width - JUMP_BTN_OFFSET, jbY = eng->height - JUMP_BTN_OFFSET;
        if (sqrtf((x-jbX)*(x-jbX) + (y-jbY)*(y-jbY)) < JUMP_BTN_SIZE) {
            if (eng->onGround) eng->velY = JUMP_FORCE;
            return 1;
        }

        // --- ЛОМАНИЕ (Зажатие) ---
        float bbX = eng->width - BREAK_BTN_X, bbY = BREAK_BTN_Y;
        if (sqrtf((x-bbX)*(x-bbX) + (y-bbY)*(y-bbY)) < ACTION_BTN_SIZE * 1.5f) {
            eng->isBreaking = true;
            return 1;
        }

        // --- СТАВЛЕНИЕ (Одиночное нажатие) ---
        float pbX = eng->width - PLACE_BTN_X, pbY = PLACE_BTN_Y;
        if (sqrtf((x-pbX)*(x-pbX) + (y-pbY)*(y-pbY)) < ACTION_BTN_SIZE * 1.5f) {
            place_block(eng);
            return 1;
        }

        // --- ДЖОЙСТИК ---
        float jx = JOY_X_OFFSET, jy = eng->height - JOY_Y_OFFSET;
        if (sqrtf((x-jx)*(x-jx) + (y-jy)*(y-jy)) < JOY_RADIUS * 1.5f) {
            eng->isMoving = true;
            eng->movePointerId = id;
            return 1;
        }

        // --- ВЗГЛЯД (Если ничего не нажато) ---
        eng->lastTouchX = x; eng->lastTouchY = y;
        eng->lookPointerId = id;
        return 1;
    }

    if (code == AMOTION_EVENT_ACTION_MOVE) {
        for (int i = 0; i < pCount; i++) {
            float x = AMotionEvent_getX(event, i);
            float y = AMotionEvent_getY(event, i);
            int id = AMotionEvent_getPointerId(event, i);

            if (id == eng->movePointerId) {
                float dx = x - JOY_X_OFFSET;
                float dy = y - (eng->height - JOY_Y_OFFSET);
                float dist = sqrtf(dx*dx + dy*dy);
                if (dist > 5.0f) {
                    float clampDist = dist > JOY_RADIUS ? JOY_RADIUS : dist;
                    eng->moveDirX = (dx / dist) * (clampDist / JOY_RADIUS);
                    eng->moveDirZ = (dy / dist) * (clampDist / JOY_RADIUS);
                }
            } else if (id == eng->lookPointerId) {
                eng->camRot[1] += (x - eng->lastTouchX) * 0.005f;
                eng->camRot[0] += (y - eng->lastTouchY) * 0.005f;
                if (eng->camRot[0] > 1.5f) eng->camRot[0] = 1.5f;
                if (eng->camRot[0] < -1.5f) eng->camRot[0] = -1.5f;
                eng->lastTouchX = x; eng->lastTouchY = y;
            }
        }
        return 1;
    }

    if (code == AMOTION_EVENT_ACTION_UP || code == AMOTION_EVENT_ACTION_POINTER_UP) {
        int pi = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int id = AMotionEvent_getPointerId(event, pi);

        if (id == eng->movePointerId) {
            eng->isMoving = false; eng->moveDirX = 0; eng->moveDirZ = 0; eng->movePointerId = -1;
        } else if (id == eng->lookPointerId) {
            eng->lookPointerId = -1;
        }
        
        // Сбрасываем ломание при любом поднятии пальца, если это был палец ломания
        float x = AMotionEvent_getX(event, pi);
        float y = AMotionEvent_getY(event, pi);
        float bbX = eng->width - BREAK_BTN_X, bbY = BREAK_BTN_Y;
        if (sqrtf((x-bbX)*(x-bbX) + (y-bbY)*(y-bbY)) < ACTION_BTN_SIZE * 2.0f) {
            eng->isBreaking = false;
            eng->miningProgress = 0;
        }
        
        // На всякий случай: если пальцев на экране 0, сбрасываем всё
        if (pCount <= 1) {
            eng->isBreaking = false;
            eng->miningProgress = 0;
        }

        return 1;
    }

    return 0;
}

#endif
