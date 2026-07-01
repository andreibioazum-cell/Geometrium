#ifndef WORLD_H
#define WORLD_H
#include "engine.h"
#include <string.h>

static unsigned int hash2d(int x, int z, int seed) {
    unsigned int h = (unsigned int)(x * 374761393 + z * 668265263 + seed);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float noise2d(int x, int z, int seed) {
    return (float)(hash2d(x, z, seed) & 0xFFFF) / 65535.0f;
}

static int get_height(int wx, int wz, int seed) {
    float fx = (float)wx, fz = (float)wz;
    float h = noise2d(wx, wz, seed) * 2.0f;
    h += sinf(fx * 0.1f) * 2.0f + cosf(fz * 0.1f) * 2.0f;
    int res = (int)h + 8;
    return (res < 1) ? 1 : ((res >= CHUNK_H) ? CHUNK_H - 1 : res);
}

static void world_to_buf(struct engine* eng, int wx, int wz, int* bx, int* bz) {
    *bx = wx - eng->loadCenterX + LOAD_RADIUS;
    *bz = wz - eng->loadCenterZ + LOAD_RADIUS;
}

static unsigned char world_block_at(struct engine* eng, int wx, int wy, int wz) {
    if (wy < 0 || wy >= CHUNK_H) return 0;
    int bx, bz; world_to_buf(eng, wx, wz, &bx, &bz);
    if (bx < 0 || bx >= WORLD_BUF || bz < 0 || bz >= WORLD_BUF) return 0;
    return eng->blocks[bx][wy][bz];
}

static void world_set_block(struct engine* eng, int wx, int wy, int wz, unsigned char val) {
    int bx, bz; world_to_buf(eng, wx, wz, &bx, &bz);
    if (bx >= 0 && bx < WORLD_BUF && bz >= 0 && bz < WORLD_BUF) eng->blocks[bx][wy][bz] = val;
    for (int i = 0; i < eng->editCount; i++) {
        if (eng->edits[i].wx == wx && eng->edits[i].wy == wy && eng->edits[i].wz == wz) {
            eng->edits[i].val = val; eng->meshDirty = true; return;
        }
    }
    if (eng->editCount < MAX_EDITS) {
        eng->edits[eng->editCount++] = (struct block_edit){wx, wy, wz, val};
    }
    eng->meshDirty = true;
}

static void load_blocks_around(struct engine* eng, int cx, int cz) {
    eng->loadCenterX = cx; eng->loadCenterZ = cz;
    memset(eng->blocks, 0, sizeof(eng->blocks));
    for (int x = 0; x < WORLD_BUF; x++) {
        for (int z = 0; z < WORLD_BUF; z++) {
            int h = get_height(cx + x - LOAD_RADIUS, cz + z - LOAD_RADIUS, eng->worldSeed);
            for (int y = 0; y < h; y++) eng->blocks[x][y][z] = BLOCK_GRASS;
        }
    }
    for (int i = 0; i < eng->editCount; i++) {
        int bx, bz; world_to_buf(eng, eng->edits[i].wx, eng->edits[i].wz, &bx, &bz);
        if (bx >= 0 && bx < WORLD_BUF && bz >= 0 && bz < WORLD_BUF)
            eng->blocks[bx][eng->edits[i].wy][bz] = eng->edits[i].val;
    }
    eng->worldLoaded = true; eng->meshDirty = true;
}

static bool raycast(struct engine* eng, int* hx, int* hy, int* hz, int* px, int* py, int* pz) {
    float dx = sinf(eng->camRot[1]) * cosf(eng->camRot[0]);
    float dy = -sinf(eng->camRot[0]);
    float dz = -cosf(eng->camRot[1]) * cosf(eng->camRot[0]);
    float x = eng->camPos[0], y = eng->camPos[1], z = eng->camPos[2];
    int lx = -999, ly = -999, lz = -999;
    for (float t = 0; t < RAY_DIST; t += RAY_STEP) {
        int wx = (int)floorf(x + dx * t + 0.5f);
        int wy = (int)floorf(y + dy * t + 0.5f);
        int wz = (int)floorf(-z + dz * t + 0.5f);
        if (world_block_at(eng, wx, wy, wz)) {
            *hx = wx; *hy = wy; *hz = wz; *px = lx; *py = ly; *pz = lz; return true;
        }
        lx = wx; ly = wy; lz = wz;
    }
    return false;
}
#endif
