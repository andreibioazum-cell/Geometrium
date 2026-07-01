#ifndef WORLD_H
#define WORLD_H

#include <math.h>
#include <string.h>
#include "engine.h"
#include "math_utils.h"

static unsigned int hash2d(int x, int z, unsigned int seed) {
    unsigned int h = (unsigned int)x * 374761393u + (unsigned int)z * 668265263u + seed;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float noise2d(int x, int z, unsigned int seed) {
    return (float)(hash2d(x, z, seed) & 0xFFFF) / 65535.0f;
}

static float smooth_noise(float fx, float fz, unsigned int seed) {
    int ix = (int)floorf(fx);
    int iz = (int)floorf(fz);
    float dx = fx - ix;
    float dz = fz - iz;
    dx = dx * dx * (3.0f - 2.0f * dx);
    dz = dz * dz * (3.0f - 2.0f * dz);
    float v00 = noise2d(ix, iz, seed);
    float v10 = noise2d(ix + 1, iz, seed);
    float v01 = noise2d(ix, iz + 1, seed);
    float v11 = noise2d(ix + 1, iz + 1, seed);
    return v00 + (v10 - v00) * dx + (v01 - v00) * dz + (v00 - v10 - v01 + v11) * dx * dz;
}

static int get_height(int wx, int wz, unsigned int seed) {
    float h = smooth_noise(wx * 0.05f, wz * 0.05f, seed);
    return (int)(h * 10.0f) + 8;
}

static void pos_to_block(float rx, float ry, float rz, int* wx, int* wy, int* wz) {
    *wx = (int)floorf(rx + 0.5f);
    *wy = (int)floorf(ry + 0.5f);
    *wz = (int)floorf(-rz + 0.5f);
}

static void world_to_buf(struct engine* eng, int wx, int wz, int* bx, int* bz) {
    *bx = wx - eng->loadCenterX + LOAD_RADIUS;
    *bz = wz - eng->loadCenterZ + LOAD_RADIUS;
}

static unsigned char world_block_at(struct engine* eng, int wx, int wy, int wz) {
    if (wy < 0 || wy >= CHUNK_H) return 0;
    int bx, bz;
    world_to_buf(eng, wx, wz, &bx, &bz);
    if (bx < 0 || bx >= WORLD_BUF || bz < 0 || bz >= WORLD_BUF) return 0;
    return eng->blocks[bx][wy][bz];
}

static void load_blocks_around(struct engine* eng, int cx, int cz) {
    eng->loadCenterX = cx;
    eng->loadCenterZ = cz;
    memset(eng->blocks, 0, WORLD_BUF * CHUNK_H * WORLD_BUF);
    for (int dx = -LOAD_RADIUS; dx <= LOAD_RADIUS; dx++) {
        for (int dz = -LOAD_RADIUS; dz <= LOAD_RADIUS; dz++) {
            int bx = dx + LOAD_RADIUS, bz = dz + LOAD_RADIUS;
            int h = get_height(cx + dx, cz + dz, eng->worldSeed);
            for (int y = 0; y < h; y++)
                eng->blocks[bx][y][bz] = (y == h - 1) ? BLOCK_GRASS : BLOCK_DIRT;
        }
    }
    // Применяем эдиты
    for (int i = 0; i < eng->editCount; i++) {
        int bx, bz;
        world_to_buf(eng, eng->edits[i].wx, eng->edits[i].wz, &bx, &bz);
        if (bx >= 0 && bx < WORLD_BUF && bz >= 0 && bz < WORLD_BUF)
            eng->blocks[bx][eng->edits[i].wy][bz] = eng->edits[i].val;
    }
    eng->worldLoaded = true;
    eng->meshDirty = true;
}

static void world_set_block(struct engine* eng, int wx, int wy, int wz, unsigned char val) {
    int bx, bz;
    world_to_buf(eng, wx, wz, &bx, &bz);
    if (bx >= 0 && bx < WORLD_BUF && bz >= 0 && bz < WORLD_BUF && wy >= 0 && wy < CHUNK_H) {
        eng->blocks[bx][wy][bz] = val;
        // Обновляем эдиты
        bool found = false;
        for (int i = 0; i < eng->editCount; i++) {
            if (eng->edits[i].wx == wx && eng->edits[i].wy == wy && eng->edits[i].wz == wz) {
                eng->edits[i].val = val; found = true; break;
            }
        }
        if (!found && eng->editCount < MAX_EDITS) {
            eng->edits[eng->editCount++] = (struct block_edit){wx, wy, wz, val};
        }
        eng->meshDirty = true;
    }
}

static void start_block_anim(struct engine* eng, int wx, int wy, int wz, unsigned char type, bool breaking) {
    eng->animActive = true;
    eng->animIsBreak = breaking;
    eng->animBlockType = type;
    eng->animBlockPos[0] = (float)wx;
    eng->animBlockPos[1] = (float)wy;
    eng->animBlockPos[2] = -(float)wz;
    eng->animTimer = breaking ? ANIM_BREAK_FRAMES : ANIM_PLACE_FRAMES;
}

static bool raycast(struct engine* eng, int* hx, int* hy, int* hz, int* px, int* py, int* pz) {
    float vx = sinf(eng->camRot[1]) * cosf(eng->camRot[0]);
    float vy = sinf(eng->camRot[0]);
    float vz = -cosf(eng->camRot[1]) * cosf(eng->camRot[0]);
    int lx = -9999, ly = -9999, lz = -9999;
    for (float t = 0; t < RAY_DIST; t += RAY_STEP) {
        int wx, wy, wz;
        pos_to_block(eng->camPos[0] + vx * t, eng->camPos[1] + vy * t, eng->camPos[2] + vz * t, &wx, &wy, &wz);
        if (world_block_at(eng, wx, wy, wz) != BLOCK_AIR) {
            *hx = wx; *hy = wy; *hz = wz;
            *px = lx; *py = ly; *pz = lz;
            return true;
        }
        lx = wx; ly = wy; lz = wz;
    }
    return false;
}

static void place_block(struct engine* eng) {
    int hx, hy, hz, px, py, pz;
    if (!raycast(eng, &hx, &hy, &hz, &px, &py, &pz)) return;
    if (px == -9999 || py < 0 || py >= CHUNK_H) return;
    
    unsigned char type = eng->invSlots[eng->selectedSlot];
    if (type == BLOCK_AIR) return;

    // Проверка коллизии с игроком
    int iwx, iwy, iwz;
    pos_to_block(eng->camPos[0], eng->camPos[1] - 0.5f, eng->camPos[2], &iwx, &iwy, &iwz);
    if (px == iwx && py == iwy && pz == iwz) return;

    world_set_block(eng, px, py, pz, type);
    start_block_anim(eng, px, py, pz, type, false);
}

static void break_block(struct engine* eng) {
    int hx, hy, hz, px, py, pz;
    if (raycast(eng, &hx, &hy, &hz, &px, &py, &pz)) {
        unsigned char type = world_block_at(eng, hx, hy, hz);
        start_block_anim(eng, hx, hy, hz, type, true);
        world_set_block(eng, hx, hy, hz, BLOCK_AIR);
    }
}

#endif
