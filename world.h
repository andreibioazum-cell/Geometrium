#ifndef WORLD_H
#define WORLD_H

#define WORLD_SIZE 16
#define CHUNK_H 8

unsigned char map[WORLD_SIZE][CHUNK_H][WORLD_SIZE];

void generate_world() {
    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int z = 0; z < WORLD_SIZE; z++) {
            int height = (int)(sinf(x * 0.5f) * 2.0f + cosf(z * 0.5f) * 2.0f) + 3;
            for (int y = 0; y < CHUNK_H; y++) {
                if (y < height) map[x][y][z] = 1; // Блок есть
                else map[x][y][z] = 0; // Воздух
            }
        }
    }
}
#endif
