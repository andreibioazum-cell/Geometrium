#ifndef WORLD_H
#define WORLD_H

#include <math.h>
#include <string.h>
#include "engine.h"

static unsigned int hash2d(int x, int z) {
    unsigned int h = (unsigned int)(x * 374761393 + z * 668265263);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float noise2d(int x, int z) {
    return (float)(hash2d(x, z) & 0xFFFF) / 65535.0f;
}

static float smooth_noise(float fx, float fz) {
    int ix = (int)floorf(fx);
    int iz = (int)floorf(fz);
    float dx = fx - ix;
    float dz = fz - iz;
    float v00 = noise2d(ix, iz);
    float v10 = noise2d(ix+1, iz);
    float v01 = noise2d(ix, iz+1);
    float v11 = noise2d(ix+1, iz+1);
    float i0 = v00 + (v10 - v00) * dx;
    float i1 = v01 + (v11 - v01) * dx;
    return i0 + (i1 - i0) * dz;
}

static int get_height(int wx, int wz) {
    float h = 0;
    h += smooth_noise(wx * 0.05f, wz * 0.05f) * 4.0f;
    h += smooth_noise(wx * 0.1f,  wz * 0.1f)  * 2.0f;
    h += smooth_noise(wx * 0.2f,  wz * 0.2f)  * 1.0f;
    int height = (int)(h) + 3;
    if (height < 1) height = 1;
    if (height >= CHUNK_H) height = CHUNK_H - 1;
    return height;
}

/* Загрузить блоки в радиусе LOAD_RADIUS вокруг (cx,cz) */
static void load_blocks_around(struct engine* eng, int cx, int cz) {
    eng->loadCenterX = cx;
    eng->loadCenterZ = cz;

    memset(eng->blocks, 0, sizeof(eng->blocks));

    for (int dx = -LOAD_RADIUS; dx <= LOAD_RADIUS; dx++) {
        for (int dz = -LOAD_RADIUS; dz <= LOAD_RADIUS; dz++) {
            int wx = cx + dx;
            int wz = cz + dz;
            int bx = dx + LOAD_RADIUS;
            int bz = dz + LOAD_RADIUS;
            int h = get_height(wx, wz);

            for (int y = 0; y < CHUNK_H; y++)
                eng->blocks[bx][y][bz] = (y < h) ? 1 : 0;
            eng->blocks[bx][0][bz] = 1;
        }
    }

    eng->worldLoaded = true;
    eng->meshDirty = true;
}

/* Получить блок из буфера по локальным координатам */
static int buf_block(struct engine* eng, int bx, int by, int bz) {
    if (by < 0 || by >= CHUNK_H) return 0;
    if (bx < 0 || bx >= WORLD_BUF || bz < 0 || bz >= WORLD_BUF)
        return 0;
    return eng->blocks[bx][by][bz];
}

/* Получить блок по мировым координатам */
static int world_block_at(struct engine* eng, int wx, int wy, int wz) {
    if (wy < 0 || wy >= CHUNK_H) return 0;
    int bx = wx - eng->loadCenterX + LOAD_RADIUS;
    int bz = wz - eng->loadCenterZ + LOAD_RADIUS;
    if (bx < 0 || bx >= WORLD_BUF || bz < 0 || bz >= WORLD_BUF)
        return 0;
    return eng->blocks[bx][wy][bz];
}

/* Обновить видимость граней */
static void update_faces(struct engine* eng) {
    for (int x = 0; x < WORLD_BUF; x++)
        for (int y = 0; y < CHUNK_H; y++)
            for (int z = 0; z < WORLD_BUF; z++) {
                if (!eng->blocks[x][y][z]) {
                    eng->faces[x][y][z] = 0;
                    continue;
                }
                unsigned char f = 0;
                if (!buf_block(eng, x+1, y, z)) f |= FACE_XP;
                if (!buf_block(eng, x-1, y, z)) f |= FACE_XN;
                if (!buf_block(eng, x, y+1, z)) f |= FACE_YP;
                if (!buf_block(eng, x, y-1, z)) f |= FACE_YN;
                if (!buf_block(eng, x, y, z+1)) f |= FACE_ZP;
                if (!buf_block(eng, x, y, z-1)) f |= FACE_ZN;
                eng->faces[x][y][z] = f;
            }
}

/* Проверить нужно ли перезагрузить блоки */
static void update_world(struct engine* eng) {
    int px = (int)floorf(eng->camPos[0]);
    int pz = (int)floorf(-eng->camPos[2]);

    if (!eng->worldLoaded) {
        load_blocks_around(eng, px, pz);
        return;
    }

    int dx = px - eng->loadCenterX;
    int dz = pz - eng->loadCenterZ;

    /* Перезагружаем когда игрок сдвинулся на 8+ блоков от центра */
    if (dx * dx + dz * dz > 64) {
        load_blocks_around(eng, px, pz);
    }
}

#endif
