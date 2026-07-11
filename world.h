#ifndef WORLD_H
#define WORLD_H

#include <math.h>
#include <string.h>
#include "engine.h"
#include "math_utils.h"

extern unsigned int game_seed;

static unsigned int hash2d(int x, int z) {
    unsigned int h = (unsigned int)(x * 374761393 + z * 668265263 + game_seed * 1103515245);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float noise2d(int x, int z) { return (float)(hash2d(x, z) & 0xFFFF) / 65535.0f; }

static int get_height(int wx, int wz) {
    float val = (noise2d(wx, wz) + noise2d(wx+1, wz)) * 0.5f;
    return (int)(val * 7.0f) + 6;
}

static void pos_to_block(float rx, float ry, float rz, int* wx, int* wy, int* wz) {
    *wx = (int)floorf(rx + 0.5f); *wy = (int)floorf(ry + 0.5f); *wz = (int)floorf(-rz + 0.5f);
}

static void world_to_buf(struct engine* eng, int wx, int wz, int* bx, int* bz) {
    *bx = wx - eng->loadCenterX + LOAD_RADIUS; *bz = wz - eng->loadCenterZ + LOAD_RADIUS;
}

static int world_block_at(struct engine* eng, int wx, int wy, int wz) {
    if (wy < 0 || wy >= CHUNK_H) return 0;
    int bx, bz; world_to_buf(eng, wx, wz, &bx, &bz);
    if (bx < 0 || bx >= WORLD_BUF || bz < 0 || bz >= WORLD_BUF) return 0;
    return eng->blocks[bx][wy][bz];
}

static void try_place_tree(struct engine* eng, int bx, int bz, int h) {
    unsigned int hsh = hash2d(eng->loadCenterX-LOAD_RADIUS+bx, eng->loadCenterZ-LOAD_RADIUS+bz);
    if ((hsh % 140) == 0 && bx > 3 && bx < WORLD_BUF-4 && bz > 3 && bz < WORLD_BUF-4) {
        int treeH = 4 + (hsh % 3);
        for (int i = 0; i < treeH; i++) eng->blocks[bx][h+i][bz] = BLOCK_WOOD;
        for (int lx = -2; lx <= 2; lx++)
            for (int lz = -2; lz <= 2; lz++)
                for (int ly = 0; ly < 3; ly++) {
                    int yy = h + treeH - 1 + ly;
                    if (yy < CHUNK_H && eng->blocks[bx+lx][yy][bz+lz] == 0)
                        eng->blocks[bx+lx][yy][bz+lz] = BLOCK_LEAVES;
                }
    }
}

static void load_blocks_around(struct engine* eng, int cx, int cz) {
    eng->loadCenterX = cx; eng->loadCenterZ = cz;
    memset(eng->blocks, 0, sizeof(eng->blocks));
    for (int dx = -LOAD_RADIUS; dx <= LOAD_RADIUS; dx++)
        for (int dz = -LOAD_RADIUS; dz <= LOAD_RADIUS; dz++) {
            int bx = dx + LOAD_RADIUS, bz = dz + LOAD_RADIUS;
            int h = get_height(cx + dx, cz + dz);
            for (int y = 0; y < h; y++) eng->blocks[bx][y][bz] = BLOCK_GRASS;
            try_place_tree(eng, bx, bz, h);
        }
    for (int i = 0; i < eng->editCount; i++) {
        int bx, bz; world_to_buf(eng, eng->edits[i].wx, eng->edits[i].wz, &bx, &bz);
        if (bx >= 0 && bx < WORLD_BUF && bz >= 0 && bz < WORLD_BUF)
            eng->blocks[bx][eng->edits[i].wy][bz] = eng->edits[i].val;
    }
    eng->worldLoaded = true; eng->meshDirty = true;
}

static void world_set_block(struct engine* eng, int wx, int wy, int wz, unsigned char val) {
    int bx, bz; world_to_buf(eng, wx, wz, &bx, &bz);
    if (bx < 0 || bx >= WORLD_BUF || bz < 0 || bz >= WORLD_BUF || wy < 0 || wy >= CHUNK_H) return;
    eng->blocks[bx][wy][bz] = val;
    if (eng->editCount < MAX_EDITS) eng->edits[eng->editCount++] = (struct block_edit){wx, wy, wz, val};
    eng->meshDirty = true;
}

static void update_faces(struct engine* eng) {
    for (int x = 0; x < WORLD_BUF; x++)
        for (int y = 0; y < CHUNK_H; y++)
            for (int z = 0; z < WORLD_BUF; z++) {
                unsigned char b = eng->blocks[x][y][z];
                if (!b) { eng->faces[x][y][z] = 0; continue; }
                unsigned char f = 0;
                // Листва НЕ скрывает грани
                #define IS_OP(bx,by,bz) (bx>=0 && bx<WORLD_BUF && by>=0 && by<CHUNK_H && bz>=0 && bz<WORLD_BUF && eng->blocks[bx][by][bz] != 0 && eng->blocks[bx][by][bz] != BLOCK_LEAVES)
                if (!IS_OP(x+1,y,z)) f|=FACE_XP; if (!IS_OP(x-1,y,z)) f|=FACE_XN;
                if (!IS_OP(x,y+1,z)) f|=FACE_YP; if (!IS_OP(x,y-1,z)) f|=FACE_YN;
                if (!IS_OP(x,y,z+1)) f|=FACE_ZP; if (!IS_OP(x,y,z-1)) f|=FACE_ZN;
                eng->faces[x][y][z] = f;
            }
}

static void update_world(struct engine* eng) {
    int px = (int)floorf(eng->camPos[0]), pz = (int)floorf(-eng->camPos[2]);
    if (!eng->worldLoaded || (px-eng->loadCenterX)*(px-eng->loadCenterX) + (pz-eng->loadCenterZ)*(pz-eng->loadCenterZ) > 81)
        load_blocks_around(eng, px, pz);
}

static bool raycast(struct engine* eng, int* hX, int* hY, int* hZ, int* pX, int* pY, int* pZ) {
    float view[16]; mat4_lookat(view, eng->camPos, eng->camRot[0], eng->camRot[1]);
    float dx = -view[2], dy = -view[6], dz = -view[10]; *pX=-9999;
    for (float t=0.1f; t<RAY_DIST; t+=RAY_STEP) {
        int wx,wy,wz; pos_to_block(eng->camPos[0]+dx*t, eng->camPos[1]+dy*t, eng->camPos[2]+dz*t, &wx,&wy,&wz);
        if (world_block_at(eng,wx,wy,wz)>0) { *hX=wx;*hY=wy;*hZ=wz; return true; }
        *pX=wx;*pY=wy;*pZ=wz;
    }
    return false;
}

static void start_block_anim(struct engine* eng, int wx, int wy, int wz, unsigned char type, bool breaking) {
    eng->animActive = true; eng->animBlockX = (float)wx; eng->animBlockY = (float)wy; eng->animBlockZ = -(float)wz;
    eng->animBlockType = type; eng->animIsBreak = breaking;
    if (breaking) eng->animBreakTimer = ANIM_BREAK_FRAMES; else eng->animPlaceTimer = ANIM_PLACE_FRAMES;
}

static void break_block(struct engine* eng) {
    int hx, hy, hz, px, py, pz;
    if (raycast(eng, &hx, &hy, &hz, &px, &py, &pz)) {
        if (hy <= 0) return;
        if (eng->miningX != hx || eng->miningY != hy || eng->miningZ != hz) { eng->miningProgress = 0; eng->miningX = hx; eng->miningY = hy; eng->miningZ = hz; }
        eng->miningProgress += 0.15f; 
        if (eng->miningProgress >= 1.0f) {
            unsigned char type = world_block_at(eng, hx, hy, hz);
            start_block_anim(eng, hx, hy, hz, type, true);
            world_set_block(eng, hx, hy, hz, BLOCK_AIR);
            for(int i=0; i<INV_SLOTS; i++) if(eng->invSlots[i]==0){ eng->invSlots[i]=type; break; }
            eng->miningProgress = 0;
        }
    } else eng->miningProgress = 0;
}

static void place_block(struct engine* eng) {
    int hx,hy,hz,px,py,pz;
    if (!raycast(eng,&hx,&hy,&hz,&px,&py,&pz) || px==-9999) return;
    unsigned char type = eng->invSlots[eng->selectedSlot];
    if (type == BLOCK_AIR) return;
    start_block_anim(eng, px, py, pz, type, false);
    world_set_block(eng,px,py,pz,type);
    eng->invSlots[eng->selectedSlot] = BLOCK_AIR;
}
#endif
