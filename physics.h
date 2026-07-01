 #ifndef PHYSICS_H
#define PHYSICS_H

#include <stdbool.h>
#include <math.h>
#include "engine.h"
#include "world.h"

static bool is_solid(struct engine* eng, float rx, float ry, float rz) {
    int wx, wy, wz;
    pos_to_block(rx, ry, rz, &wx, &wy, &wz);
    return world_block_at(eng, wx, wy, wz) > 0;
}

static bool check_box(struct engine* eng, float x, float y, float z) {
    return is_solid(eng, x-PLAYER_W, y, z-PLAYER_W) ||
           is_solid(eng, x+PLAYER_W, y, z-PLAYER_W) ||
           is_solid(eng, x-PLAYER_W, y, z+PLAYER_W) ||
           is_solid(eng, x+PLAYER_W, y, z+PLAYER_W);
}

static bool check_ground(struct engine* eng, float x, float footY, float z) {
    return check_box(eng, x, footY - 0.05f, z);
}

static bool check_ceiling(struct engine* eng, float x, float headY, float z) {
    return check_box(eng, x, headY, z);
}

static bool check_wall(struct engine* eng, float x, float eyeY, float z) {
    float foot = eyeY - EYE_H;
    return check_box(eng, x, foot+0.1f, z) || check_box(eng, x, foot+0.6f, z) ||
           check_box(eng, x, foot+1.2f, z) || check_box(eng, x, foot+1.8f, z);
}

static bool check_inside(struct engine* eng, float x, float eyeY, float z) {
    return is_solid(eng, x, eyeY, z) || is_solid(eng, x-PLAYER_W, eyeY, z) ||
           is_solid(eng, x+PLAYER_W, eyeY, z) || is_solid(eng, x, eyeY, z-PLAYER_W) ||
           is_solid(eng, x, eyeY, z+PLAYER_W);
}

static void apply_physics(struct engine* eng) {
    eng->velY -= GRAVITY;
    if (eng->velY < TERM_VEL) eng->velY = TERM_VEL;

    float nextY = eng->camPos[1] + eng->velY;
    eng->onGround = false;
    if (check_ground(eng, eng->camPos[0], eng->camPos[1]-EYE_H, eng->camPos[2]))
        eng->onGround = true;

    if (eng->velY <= 0) {
        if (check_ground(eng, eng->camPos[0], nextY-EYE_H, eng->camPos[2])) {
            eng->velY = 0; eng->onGround = true;
        } else eng->camPos[1] = nextY;
    } else {
        if (check_ceiling(eng, eng->camPos[0], nextY+HEAD_MARGIN, eng->camPos[2]))
            eng->velY = 0;
        else eng->camPos[1] = nextY;
    }

    if (eng->isMoving && (eng->moveDirX != 0 || eng->moveDirZ != 0)) {
        float speed = 0.08f, yaw = eng->camRot[1];
        float fX = sinf(yaw), fZ = -cosf(yaw), rX = cosf(yaw), rZ = sinf(yaw);
        float dx = (fX * -eng->moveDirZ + rX * eng->moveDirX) * speed;
        float dz = (fZ * -eng->moveDirZ + rZ * eng->moveDirX) * speed;
        if (!check_wall(eng, eng->camPos[0]+dx, eng->camPos[1], eng->camPos[2]))
            eng->camPos[0] += dx;
        if (!check_wall(eng, eng->camPos[0], eng->camPos[1], eng->camPos[2]+dz))
            eng->camPos[2] += dz;
    }

    if (check_inside(eng, eng->camPos[0], eng->camPos[1], eng->camPos[2]))
        for (int i = 0; i < 10; i++) {
            eng->camPos[1] += 0.1f;
            if (!check_inside(eng, eng->camPos[0], eng->camPos[1], eng->camPos[2])) break;
        }

    if (eng->camPos[1] < -10.0f) {
        eng->camPos[1] = (float)get_height((int)floorf(eng->camPos[0]),
                                            (int)floorf(-eng->camPos[2])) + 2.5f;
        eng->velY = 0;
    }
}

#endif
