#ifndef PHYSICS_H
#define PHYSICS_H
#include "engine.h"
#include "world.h"

static bool check_box(struct engine* eng, float x, float y, float z) {
    float w = PLAYER_W;
    int coords[4][2] = {{1,1}, {1,-1}, {-1,1}, {-1,-1}};
    for(int i=0; i<4; i++) {
        if(world_block_at(eng, (int)floorf(x + coords[i][0]*w + 0.5f), (int)floorf(y + 0.5f), (int)floorf(-z + coords[i][1]*w + 0.5f))) return true;
    }
    return false;
}

static void apply_physics(struct engine* eng) {
    eng->velY -= GRAVITY;
    if (eng->velY < TERM_VEL) eng->velY = TERM_VEL;
    
    float ny = eng->camPos[1] + eng->velY;
    if (eng->velY < 0) {
        if (check_box(eng, eng->camPos[0], ny - EYE_H, eng->camPos[2])) {
            eng->velY = 0; eng->onGround = true;
        } else { eng->camPos[1] = ny; eng->onGround = false; }
    } else {
        if (check_box(eng, eng->camPos[0], ny + HEAD_MARGIN, eng->camPos[2])) eng->velY = 0;
        else eng->camPos[1] = ny;
    }

    if (eng->isMoving) {
        float s = 0.08f, yaw = eng->camRot[1];
        float dx = (sinf(yaw) * -eng->moveDirZ + cosf(yaw) * eng->moveDirX) * s;
        float dz = ((-cosf(yaw)) * -eng->moveDirZ + sinf(yaw) * eng->moveDirX) * s;
        if (!check_box(eng, eng->camPos[0] + dx, eng->camPos[1] - EYE_H + 0.1f, eng->camPos[2])) eng->camPos[0] += dx;
        if (!check_box(eng, eng->camPos[0], eng->camPos[1] - EYE_H + 0.1f, eng->camPos[2] + dz)) eng->camPos[2] += dz;
    }
}
#endif
