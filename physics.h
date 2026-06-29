#ifndef PHYSICS_H
#define PHYSICS_H

#include <stdbool.h>
#include <math.h>
#include "engine.h"
#include "world.h"

static bool is_solid(float x, float y, float z) {
    int ix = (int)floorf(x + 0.5f);
    int iy = (int)floorf(y + 0.5f);
    int iz = (int)floorf(-z + 0.5f);
    if (ix < 0 || ix >= WORLD_SIZE ||
        iy < 0 || iy >= CHUNK_H    ||
        iz < 0 || iz >= WORLD_SIZE)
        return false;
    return map[ix][iy][iz] > 0;
}

static bool check_box(float x, float y, float z) {
    return is_solid(x - PLAYER_W, y, z - PLAYER_W) ||
           is_solid(x + PLAYER_W, y, z - PLAYER_W) ||
           is_solid(x - PLAYER_W, y, z + PLAYER_W) ||
           is_solid(x + PLAYER_W, y, z + PLAYER_W);
}

static bool check_ground(float x, float footY, float z) {
    return check_box(x, footY - 0.05f, z);
}

static bool check_ceiling(float x, float headY, float z) {
    return check_box(x, headY, z);
}

static bool check_wall(float x, float eyeY, float z) {
    float foot = eyeY - EYE_H;
    return check_box(x, foot + 0.1f, z) ||
           check_box(x, foot + 0.6f, z) ||
           check_box(x, foot + 1.2f, z) ||
           check_box(x, foot + 1.8f, z);
}

static bool check_inside(float x, float eyeY, float z) {
    return is_solid(x, eyeY, z) ||
           is_solid(x - PLAYER_W, eyeY, z) ||
           is_solid(x + PLAYER_W, eyeY, z) ||
           is_solid(x, eyeY, z - PLAYER_W) ||
           is_solid(x, eyeY, z + PLAYER_W);
}

static void apply_physics(struct engine* eng) {
    eng->velY -= 0.012f;
    if (eng->velY < -0.5f) eng->velY = -0.5f;

    float nextY = eng->camPos[1] + eng->velY;
    float nextFoot = nextY - EYE_H;
    float nextHead = nextY + HEAD_MARGIN;

    eng->onGround = false;

    /* Также проверяем "почти на земле" для надёжного прыжка */
    if (check_ground(eng->camPos[0], eng->camPos[1] - EYE_H, eng->camPos[2])) {
        eng->onGround = true;
    }

    if (eng->velY <= 0) {
        if (check_ground(eng->camPos[0], nextFoot, eng->camPos[2])) {
            eng->velY = 0;
            eng->onGround = true;
        } else {
            eng->camPos[1] = nextY;
        }
    } else {
        if (check_ceiling(eng->camPos[0], nextHead, eng->camPos[2])) {
            eng->velY = 0;
        } else {
            eng->camPos[1] = nextY;
        }
    }

    if (eng->isMoving && (eng->moveDirX != 0 || eng->moveDirZ != 0)) {
        float speed = 0.08f;
        float yaw = eng->camRot[1];
        float fX = sinf(yaw), fZ = -cosf(yaw);
        float rX = cosf(yaw), rZ = sinf(yaw);
        float dx = (fX * -eng->moveDirZ + rX * eng->moveDirX) * speed;
        float dz = (fZ * -eng->moveDirZ + rZ * eng->moveDirX) * speed;

        if (!check_wall(eng->camPos[0] + dx, eng->camPos[1], eng->camPos[2]))
            eng->camPos[0] += dx;
        if (!check_wall(eng->camPos[0], eng->camPos[1], eng->camPos[2] + dz))
            eng->camPos[2] += dz;
    }

    if (check_inside(eng->camPos[0], eng->camPos[1], eng->camPos[2])) {
        for (int i = 0; i < 10; i++) {
            eng->camPos[1] += 0.1f;
            if (!check_inside(eng->camPos[0], eng->camPos[1], eng->camPos[2]))
                break;
        }
    }

    if (eng->camPos[1] < -10.0f) {
        eng->camPos[0] = WORLD_SIZE / 2.0f;
        eng->camPos[2] = -(WORLD_SIZE / 2.0f);
        eng->camPos[1] = get_spawn_y((int)eng->camPos[0],
                                      (int)(-eng->camPos[2]));
        eng->velY = 0;
    }
}

#endif
