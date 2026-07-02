#ifndef WORLD_H
#define WORLD_H
#include "engine.h"
#include "math_utils.h"

extern unsigned int game_seed;

static unsigned int hash2d(int x, int z) {
    unsigned int h = (unsigned int)(x * 374761393 + z * 668265263 + game_seed * 1103515245);
    h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16);
}
static float noise2d(int x, int z) { return (float)(hash2d(x, z) & 0xFFFF) / 65535.0f; }
static float smooth_noise(float fx, float fz) {
    int ix = (int)floorf(fx), iz = (int)floorf(fz);
    float dx = fx - ix, dz = fz - iz;
    dx = dx * dx * (3.0f - 2.0f * dx); dz = dz * dz * (3.0f - 2.0f * dz);
    float v00 = noise2d(ix, iz), v10 = noise2d(ix+1, iz), v01 = noise2d(ix, iz+1), v11 = noise2d(ix+1, iz+1);
    return v00 + (v10-v00)*dx + (v01-v00)*dz + (v00-v10-v01+v11)*dx*dz;
}
static float fbm_noise(float x, float z) {
    return smooth_noise(x*0.015f, z*0.015f)*16.0f + smooth_noise(x*0.03f, z*0.03f)*8.0f;
}
static int get_height(int wx, int wz) {
    int h = (int)fbm_noise((float)wx, (float)wz) + 8;
    return (h < 2) ? 2 : (h >= CHUNK_H ? CHUNK_H-1 : h);
}
static void pos_to_block(float rx, float ry, float rz, int* wx, int* wy, int* wz) {
    *wx = (int)floorf(rx + 0.5f); *wy = (int)floorf(ry + 0.5f); *wz = (int)floorf(-rz + 0.5f);
}
static void world_to_buf(struct engine* eng, int wx, int wz, int* bx, int* bz) {
    *bx = wx - eng->loadCenterX + LOAD_RADIUS; *bz = wz - eng->loadCenterZ + LOAD_RADIUS;
}
static int buf_block(struct engine* eng, int bx, int by, int bz) {
    if (by<0 || by>=CHUNK_H || bx<0 || bx>=WORLD_BUF || bz<0 || bz>=WORLD_BUF) return 0;
    return eng->blocks[bx][by][bz];
}
static int world_block_at(struct engine* eng, int wx, int wy, int wz) {
    int bx, bz; world_to_buf(eng, wx, wz, &bx, &bz); return buf_block(eng, bx, wy, bz);
}
static void world_set_block(struct engine* eng, int wx, int wy, int wz, unsigned char val) {
    int bx, bz; world_to_buf(eng, wx, wz, &bx, &bz);
    if (bx>=0 && bx<WORLD_BUF && bz>=0 && bz<WORLD_BUF) eng->blocks[bx][wy][bz] = val;
    if (eng->editCount < MAX_EDITS) eng->edits[eng->editCount++] = (struct block_edit){wx, wy, wz, val};
    eng->meshDirty = true;
}
static void load_blocks_around(struct engine* eng, int cx, int cz) {
    eng->loadCenterX = cx; eng->loadCenterZ = cz;
    memset(eng->blocks, 0, sizeof(eng->blocks));
    for (int dx=-LOAD_RADIUS; dx<=LOAD_RADIUS; dx++)
        for (int dz=-LOAD_RADIUS; dz<=LOAD_RADIUS; dz++) {
            int h = get_height(cx+dx, cz+dz);
            for (int y=0; y<h; y++) eng->blocks[dx+LOAD_RADIUS][y][dz+LOAD_RADIUS] = BLOCK_GRASS;
        }
    for (int i=0; i<eng->editCount; i++) {
        int bx, bz; world_to_buf(eng, eng->edits[i].wx, eng->edits[i].wz, &bx, &bz);
        if (bx>=0 && bx<WORLD_BUF && bz>=0 && bz<WORLD_BUF) eng->blocks[bx][eng->edits[i].wy][bz] = eng->edits[i].val;
    }
    eng->worldLoaded = true; eng->meshDirty = true;
}
static void update_faces(struct engine* eng) {
    for (int x=0; x<WORLD_BUF; x++)
        for (int y=0; y<CHUNK_H; y++)
            for (int z=0; z<WORLD_BUF; z++) {
                if (!eng->blocks[x][y][z]) { eng->faces[x][y][z]=0; continue; }
                unsigned char f=0;
                if (!buf_block(eng,x+1,y,z)) f|=FACE_XP; if (!buf_block(eng,x-1,y,z)) f|=FACE_XN;
                if (!buf_block(eng,x,y+1,z)) f|=FACE_YP; if (!buf_block(eng,x,y-1,z)) f|=FACE_YN;
                if (!buf_block(eng,x,y,z+1)) f|=FACE_ZP; if (!buf_block(eng,x,y,z-1)) f|=FACE_ZN;
                eng->faces[x][y][z]=f;
            }
}
static void update_world(struct engine* eng) {
    int px = (int)floorf(eng->camPos[0]), pz = (int)floorf(-eng->camPos[2]);
    if (!eng->worldLoaded || (px-eng->loadCenterX)*(px-eng->loadCenterX)+(pz-eng->loadCenterZ)*(pz-eng->loadCenterZ) > 36)
        load_blocks_around(eng, px, pz);
}
static bool raycast(struct engine* eng, int* hx, int* hy, int* hz, int* px, int* py, int* pz) {
    float v[16]; mat4_lookat(v, eng->camPos, eng->camRot[0], eng->camRot[1]);
    float dx=-v[2], dy=-v[6], dz=-v[10];
    int lx=-999, ly=-999, lz=-999;
    for (float t=0.1f; t<RAY_DIST; t+=RAY_STEP) {
        int wx, wy, wz; pos_to_block(eng->camPos[0]+dx*t, eng->camPos[1]+dy*t, eng->camPos[2]+dz*t, &wx, &wy, &wz);
        if (wx==lx && wy==ly && wz==lz) continue;
        if (world_block_at(eng, wx, wy, wz)>0) { *hx=wx; *hy=wy; *hz=wz; *px=lx; *py=ly; *pz=lz; return true; }
        lx=wx; ly=wy; lz=wz;
    }
    return false;
}
#endif
