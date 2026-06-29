#ifndef WORLD_H
#define WORLD_H
#include <math.h>

#define WORLD_SIZE 16
#define CHUNK_H 10

unsigned char map[WORLD_SIZE][CHUNK_H][WORLD_SIZE];

static void generate_world() {
    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int z = 0; z < WORLD_SIZE; z++) {
            // Генерируем ландшафт
            int height = (int)(sinf(x * 0.3f) * 2.0f + cosf(z * 0.3f) * 2.0f) + 4;
            for (int y = 0; y < CHUNK_H; y++) {
                map[x][y][z] = (y < height) ? 1 : 0;
            }
            // Гарантированный пол на уровне 0, чтобы не падать в пустоту
            map[x][0][z] = 1;
        }
    }
}

// Находит верхнюю точку для спавна
static float get_spawn_y(int x, int z) {
    for (int y = CHUNK_H - 1; y >= 0; y--) {
        if (map[x][y][z]) return (float)y + 2.0f; // Возвращаем высоту над блоком
    }
    return 4.0f;
}
#endif
