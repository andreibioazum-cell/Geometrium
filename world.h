#ifndef WORLD_H
#define WORLD_H
#include <math.h>

#define WORLD_SIZE 12
#define CHUNK_H 6

unsigned char map[WORLD_SIZE][CHUNK_H][WORLD_SIZE];

static void generate_world() {
    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int z = 0; z < WORLD_SIZE; z++) {
            int height = (int)(sinf(x * 0.4f) * 1.5f + cosf(z * 0.4f) * 1.5f) + 2;
            for (int y = 0; y < CHUNK_H; y++) {
                map[x][y][z] = (y < height) ? 1 : 0;
            }
        }
    }
}
#endif
