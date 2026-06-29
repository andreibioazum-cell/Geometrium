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

// Проверяет 4 угла хитбокса на высоте y
static bool check_collision_at(float x, float y, float z) {
    return is_solid(x-PLAYER_W, y, z-PLAYER_W) ||
           is_solid(x+PLAYER_W, y, z-PLAYER_W) ||
           is_solid(x-PLAYER_W, y, z+PLAYER_W) ||
           is_solid(x+PLAYER_W, y, z+PLAYER_W);
}

static bool check_ground(float x, float footY, float z) {
    return check_collision_at(x, footY - 0.05f, z);
}

static bool check_ceiling(float x, float headY, float z) {
    return check_collision_at(x, headY, z);
}

// Стены — 4 высоты тела
static bool check_wall(float x, float eyeY, float z) {
    float footY = eyeY - EYE_H;
    return check_collision_at(x, footY + 0.1f, z) ||
           check_collision_at(x, footY + 0.6f, z) ||
           check_collision_at(x, footY + 1.2f, z) ||
           check_collision_at(x, footY + 1.8f, z);
}

static bool check_head_inside(float x, float eyeY, float z) {
    return is_solid(x, eyeY, z) ||
           is_solid(x-PLAYER_W, eyeY, z) ||
           is_solid(x+PLAYER_W, eyeY, z) ||
           is_solid(x, eyeY, z-PLAYER_W) ||
           is_solid(x, eyeY, z+PLAYER_W);
}

static void apply_physics(struct engine* eng) {
    eng->velY -= 0.012f;
    if (eng->velY < -0.5f) eng->velY = -0.5f;

    float nextEyeY = eng->camPos[1] + eng->velY;
    float nextFootY = nextEyeY - EYE_H;
    float nextHeadY = nextEyeY + HEAD_MARGIN;

    eng->onGround = false;

    if (eng->velY <= 0) {
        if (check_ground(eng->camPos[0], nextFootY, eng->camPos[2])) {
            eng->velY = 0;
            eng->onGround = true;
        } else {
            eng->camPos[1] = nextEyeY;
        }
    } else {
        if (check_ceiling(eng->camPos[0], nextHeadY, eng->camPos[2])) {
            eng->velY = 0;
        } else {
            eng->camPos[1] = nextEyeY;
        }
    }

    // Горизонтальное движение
    if (eng->isMoving && (eng->moveDirX != 0 || eng->moveDirZ != 0)) {
        float speed = 0.08f;
        float yaw = eng->camRot[1];
        float fX = sinf(yaw), fZ = -cosf(yaw);
        float rX = cosf(yaw), rZ = sinf(yaw);
        float dx = (fX * -eng->moveDirZ + rX * eng->moveDirX) * speed;
        float dz = (fZ * -eng->moveDirZ + rZ * eng->moveDirX) * speed;

        if (!check_wall(eng->camPos[0]+dx, eng->camPos[1], eng->camPos[2]))
            eng->camPos[0] += dx;
        if (!check_wall(eng->camPos[0], eng->camPos[1], eng->camPos[2]+dz))
            eng->camPos[2] += dz;
    }

    // Антипроникновение
    if (check_head_inside(eng->camPos[0], eng->camPos[1], eng->camPos[2])) {
        for (int i = 0; i < 10; i++) {
            eng->camPos[1] += 0.1f;
            if (!check_head_inside(eng->camPos[0], eng->camPos[1],
                                    eng->camPos[2]))
                break;
        }
    }

    // Респавн
    if (eng->camPos[1] < -10.0f) {
        eng->camPos[0] = WORLD_SIZE / 2.0f;
        eng->camPos[2] = -(WORLD_SIZE / 2.0f);
        eng->camPos[1] = get_spawn_y((int)eng->camPos[0],
                                      (int)(-eng->camPos[2]));
        eng->velY = 0;
    }
}

#endif
