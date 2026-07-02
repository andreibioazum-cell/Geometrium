#ifndef INPUT_H
#define INPUT_H
#include "engine.h"
#include "world.h"

static int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;
    int action = AMotionEvent_getAction(event);
    int code = action & AMOTION_EVENT_ACTION_MASK;
    int pi = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    float x = AMotionEvent_getX(event, pi), y = AMotionEvent_getY(event, pi);
    int id = AMotionEvent_getPointerId(event, pi);

    if (code == AMOTION_EVENT_ACTION_DOWN || code == AMOTION_EVENT_ACTION_POINTER_DOWN) {
        if (x < eng->width/2) { eng->movePointerId=id; eng->isMoving=true; }
        else { eng->lookPointerId=id; eng->lastTouchX=x; eng->lastTouchY=y; }
        return 1;
    }
    if (code == AMOTION_EVENT_ACTION_MOVE) {
        for(int i=0; i<AMotionEvent_getPointerCount(event); i++) {
            int mid = AMotionEvent_getPointerId(event, i);
            float mx = AMotionEvent_getX(event, i), my = AMotionEvent_getY(event, i);
            if (mid == eng->movePointerId) {
                eng->moveDirX = (mx - JOY_X_OFFSET)/JOY_RADIUS; 
                eng->moveDirZ = (my - (eng->height-JOY_Y_OFFSET))/JOY_RADIUS;
            }
            if (mid == eng->lookPointerId) {
                eng->camRot[1] += (mx - eng->lastTouchX) * 0.005f;
                eng->camRot[0] += (my - eng->lastTouchY) * 0.005f;
                eng->lastTouchX = mx; eng->lastTouchY = my;
            }
        }
        return 1;
    }
    if (code == AMOTION_EVENT_ACTION_UP || code == AMOTION_EVENT_ACTION_POINTER_UP) {
        if (id == eng->movePointerId) { eng->isMoving=false; eng->moveDirX=0; eng->moveDirZ=0; eng->movePointerId=-1; }
        if (id == eng->lookPointerId) eng->lookPointerId=-1;
        return 1;
    }
    return 0;
}
#endif
