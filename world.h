#ifndef WORLD_H
#define WORLD_H

#include <math.h>
#include <string.h>
#include "engine.h"

/* Простой хеш для шума */
static unsigned int hash2d(int x, int z) {
    unsigned int h = (unsigned int)(x * 374761393 + z * 668265263);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float noise2d(int x, int z) {
    return (float)(hash2d(x, z) & 0xFFFF) / 65535.0f;
}

/* Интерполированный шум */
static float smooth_noise(float fx, float fz) {
    int ix = (int)floorf(fx);
    int iz = (int)floorf(fz);
    float dx = fx - ix;
    float dz = fz - iz;

    float v00 = noise2d(ix, iz);
    float v10 = noise2d(ix + 1, iz);
    float v01 = noise2d(ix, iz + 1);
    float v11 = noise2d(ix + 1, iz + 1);

    float i0 = v00 + (v10 - v00) * dx;
    float i1 = v01 + (v11 - v01) * dx;

    return i0 + (i1 - i0) * dz;
}

static int get_height(int wx, int wz) {
    float h = 0;
    h += smooth_noise(wx * 0.05f, wz * 0.05f) * 4.0f;
    h += smooth_noise(wx * 0.1f, wz * 0.1f) * 2.0f;
    h += smooth_noise(wx * 0.2f, wz * 0.2f) * 1.0f;
    int height = (int)(h) + 3;
    if (height < 1) height = 1;
    if (height >= CHUNK_H) height = CHUNK_H - 1;
    return height;
}

/* Генерация блоков одного чанка */
static void generate_chunk(struct chunk* ch) {
    memset(ch->blocks, 0, sizeof(ch->blocks));
    for (int lx = 0; lx < CHUNK_SIZE; lx++) {
        for (int lz = 0; lz < CHUNK_SIZE; lz++) {
            int wx = ch->cx * CHUNK_SIZE + lx;
            int wz = ch->cz * CHUNK_SIZE + lz;
            int h = get_height(wx, wz);
            for (int y = 0; y < CHUNK_H; y++) {
                ch->blocks[lx][y][lz] = (y < h) ? 1 : 0;
            }
            ch->blocks[lx][0][lz] = 1;
        }
    }
    ch->meshDirty = true;
}

/* Получить блок из конкретного чанка */
static int chunk_block_at(struct chunk* ch, int lx, int ly, int lz) {
    if (ly < 0 || ly >= CHUNK_H) return 0;
    if (lx < 0 || lx >= CHUNK_SIZE || lz < 0 || lz >= CHUNK_SIZE)
        return 0;
    return ch->blocks[lx][ly][lz];
}

/* Обновить видимость граней */
static void update_chunk_faces(struct chunk* ch) {
    for (int x = 0; x < CHUNK_SIZE; x++)
        for (int y = 0; y < CHUNK_H; y++)
            for (int z = 0; z < CHUNK_SIZE; z++) {
                if (!ch->blocks[x][y][z]) {
                    ch->faces[x][y][z] = 0;
                    continue;
                }
                unsigned char f = 0;
                if (!chunk_block_at(ch, x+1, y, z)) f |= FACE_XP;
                if (!chunk_block_at(ch, x-1, y, z)) f |= FACE_XN;
                if (!chunk_block_at(ch, x, y+1, z)) f |= FACE_YP;
                if (!chunk_block_at(ch, x, y-1, z)) f |= FACE_YN;
                if (!chunk_block_at(ch, x, y, z+1)) f |= FACE_ZP;
                if (!chunk_block_at(ch, x, y, z-1)) f |= FACE_ZN;
                ch->faces[x][y][z] = f;
            }
}

/* Поиск чанка по координатам */
static struct chunk* find_chunk(struct engine* eng, int cx, int cz) {
    for (int i = 0; i < MAX_CHUNKS; i++) {
        if (eng->chunks[i].active &&
            eng->chunks[i].cx == cx &&
            eng->chunks[i].cz == cz)
            return &eng->chunks[i];
    }
    return NULL;
}

/* Получить блок по мировым координатам */
static int world_block_at(struct engine* eng, int wx, int wy, int wz) {
    if (wy < 0 || wy >= CHUNK_H) return 0;

    int cx, cz, lx, lz;

    if (wx >= 0)
        cx = wx / CHUNK_SIZE;
    else
        cx = (wx - CHUNK_SIZE + 1) / CHUNK_SIZE;

    if (wz >= 0)
        cz = wz / CHUNK_SIZE;
    else
        cz = (wz - CHUNK_SIZE + 1) / CHUNK_SIZE;

    lx = wx - cx * CHUNK_SIZE;
    lz = wz - cz * CHUNK_SIZE;

    struct chunk* ch = find_chunk(eng, cx, cz);
    if (!ch) return 0;

    return ch->blocks[lx][wy][lz];
}

/* Найти высоту спавна */
static float get_spawn_y_world(struct engine* eng, int wx, int wz) {
    for (int y = CHUNK_H - 1; y >= 0; y--) {
        if (world_block_at(eng, wx, y, wz))
            return (float)y + 2.5f;
    }
    return 5.0f;
}

/* Загрузить/выгрузить чанки вокруг игрока */
static void update_chunks(struct engine* eng) {
    int pcx, pcz;
    float px = eng->camPos[0];
    float pz = -eng->camPos[2];

    if (px >= 0)
        pcx = (int)floorf(px) / CHUNK_SIZE;
    else
        pcx = ((int)floorf(px) - CHUNK_SIZE + 1) / CHUNK_SIZE;

    if (pz >= 0)
        pcz = (int)floorf(pz) / CHUNK_SIZE;
    else
        pcz = ((int)floorf(pz) - CHUNK_SIZE + 1) / CHUNK_SIZE;

    if (eng->chunksReady &&
        pcx == eng->lastChunkX &&
        pcz == eng->lastChunkZ)
        return;

    eng->lastChunkX = pcx;
    eng->lastChunkZ = pcz;

    /* Деактивировать далёкие чанки */
    for (int i = 0; i < MAX_CHUNKS; i++) {
        if (!eng->chunks[i].active) continue;
        int dx = eng->chunks[i].cx - pcx;
        int dz = eng->chunks[i].cz - pcz;
        if (dx < -VIEW_DIST || dx > VIEW_DIST ||
            dz < -VIEW_DIST || dz > VIEW_DIST) {
            if (eng->chunks[i].vbo) {
                glDeleteBuffers(1, &eng->chunks[i].vbo);
                eng->chunks[i].vbo = 0;
            }
            eng->chunks[i].active = false;
        }
    }

    /* Активировать новые чанки */
    for (int dx = -VIEW_DIST; dx <= VIEW_DIST; dx++) {
        for (int dz = -VIEW_DIST; dz <= VIEW_DIST; dz++) {
            int cx = pcx + dx;
            int cz = pcz + dz;

            if (find_chunk(eng, cx, cz)) continue;

            /* Найти свободный слот */
            int slot = -1;
            for (int i = 0; i < MAX_CHUNKS; i++) {
                if (!eng->chunks[i].active) {
                    slot = i;
                    break;
                }
            }
            if (slot < 0) continue;

            struct chunk* ch = &eng->chunks[slot];
            ch->cx = cx;
            ch->cz = cz;
            ch->active = true;
            ch->vbo = 0;
            ch->faceCount = 0;
            generate_chunk(ch);
            update_chunk_faces(ch);
        }
    }

    eng->chunksReady = true;
}

#endif
