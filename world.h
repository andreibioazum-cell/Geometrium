#ifndef WORLD_H
#define WORLD_H

#include <math.h>

#define WORLD_SIZE 16
#define CHUNK_H    10

#define FACE_PX 0x01
#define FACE_NX 0x02
#define FACE_PY 0x04
#define FACE_NY 0x08
#define FACE_PZ 0x10
#define FACE_NZ 0x20

extern unsigned char map[WORLD_SIZE][CHUNK_H][WORLD_SIZE];
extern unsigned char face_visible[WORLD_SIZE][CHUNK_H][WORLD_SIZE];

static int block_at(int x, int y, int z) {
    if (x < 0 || x >= WORLD_SIZE ||
        y < 0 || y >= CHUNK_H    ||
        z < 0 || z >= WORLD_SIZE)
        return 0;
    return map[x][y][z];
}

static void update_face_visibility(void) {
    for (int x = 0; x < WORLD_SIZE; x++)
        for (int y = 0; y < CHUNK_H; y++)
            for (int z = 0; z < WORLD_SIZE; z++) {
                if (!map[x][y][z]) { face_visible[x][y][z] = 0; continue; }
                unsigned char f = 0;
                if (!block_at(x+1,y,z)) f |= FACE_PX;
                if (!block_at(x-1,y,z)) f |= FACE_NX;
                if (!block_at(x,y+1,z)) f |= FACE_PY;
                if (!block_at(x,y-1,z)) f |= FACE_NY;
                if (!block_at(x,y,z+1)) f |= FACE_PZ;
                if (!block_at(x,y,z-1)) f |= FACE_NZ;
                face_visible[x][y][z] = f;
            }
}

static void generate_world(void) {
    for (int x = 0; x < WORLD_SIZE; x++)
        for (int z = 0; z < WORLD_SIZE; z++) {
            int height = (int)(sinf(x*0.3f)*2.0f + cosf(z*0.3f)*2.0f) + 4;
            if (height < 1) height = 1;
            if (height >= CHUNK_H) height = CHUNK_H - 1;
            for (int y = 0; y < CHUNK_H; y++)
                map[x][y][z] = (y < height) ? 1 : 0;
            map[x][0][z] = 1;
        }
    update_face_visibility();
}

static float get_spawn_y(int x, int z) {
    if (x < 0) x = 0;
    if (x >= WORLD_SIZE) x = WORLD_SIZE - 1;
    if (z < 0) z = 0;
    if (z >= WORLD_SIZE) z = WORLD_SIZE - 1;
    for (int y = CHUNK_H - 1; y >= 0; y--)
        if (map[x][y][z]) return (float)y + 2.5f;
    return 5.0f;
}

#endif
