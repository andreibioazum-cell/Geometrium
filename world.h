#ifndef WORLD_H
#define WORLD_H

#include "engine.h"

static void init_world(struct engine* eng) {
    for(int x=0; x<WORLD_BUF; x++) {
        for(int z=0; z<WORLD_BUF; z++) {
            eng->blocks[x][0][z] = 1;
        }
    }
}
#endif
