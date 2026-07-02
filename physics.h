#ifndef PHYSICS_H
#define PHYSICS_H
#include "engine.h"
#include "world.h"

static bool check_box(struct engine* eng, float x, float y, float z) {
    int wx, wy, wz;
    float points[4][2] = {{-PLAYER_W, -PLAYER_W}, {PLAYER_W, -PLAYER_W}, {-PLAYER_W, PLAYER_W}, {PLAYER_W, PLAYER_W}};
    for(int i=0; i<4; i++) {
        pos_to_block(x+points[i][0], y, z+points[i][1], &wx, &wy, &wz);
        if (world_block_at(eng, wx, wy, wz) > 0) return true;
    }
    return false;
}
static void apply_physics(struct engine* eng) {
    eng->velY -= GRAVITY; if (eng->velY < TERM_VEL) eng->velY = TERM_VEL;
    float nextY = eng->camPos[1] + eng->velY;
    if (eng->velY <= 0 && check_box(eng, eng->camPos[0], nextY-EYE_H, eng->camPos[2])) {
        eng->velY = 0; eng->onGround = true;
    } else {
        eng->camPos[1] = nextY; eng->onGround = false;
    }
    if (eng->isMoving) {
        float s=0.076f, yaw=eng->camRot[1];
        float dx = (sinf(yaw)*-eng->moveDirZ + cosf(yaw)*eng->moveDirX)*s;
        float dz = (-cosf(yaw)*-eng->moveDirZ + sinf(yaw)*eng->moveDirX)*s;
        if (!check_box(eng, eng->camPos[0]+dx, eng->camPos[1]-EYE_H+0.1f, eng->camPos[2])) eng->camPos[0]+=dx;
        if (!check_box(eng, eng->camPos[0], eng->camPos[1]-EYE_H+0.1f, eng->camPos[2]+dz)) eng->camPos[2]+=dz;
    }
}
#endif
