#ifndef RENDER_H
#define RENDER_H

#include <GLES2/gl2.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "world.h"
#include "math_utils.h"

// Строим VBO из видимых граней
static void rebuild_world_vbo(struct engine* eng) {
    int faceCount = 0;
    for (int x = 0; x < WORLD_SIZE; x++)
        for (int y = 0; y < CHUNK_H; y++)
            for (int z = 0; z < WORLD_SIZE; z++) {
                unsigned char f = face_visible[x][y][z];
                for (int b = 0; b < 6; b++)
                    if (f & (1 << b)) faceCount++;
            }

    eng->visibleFaceCount = faceCount;
    if (faceCount == 0) return;

    int floatCount = faceCount * 6 * 5; // 6 вершин * 5 компонентов
    float* buf = (float*)malloc(floatCount * sizeof(float));
    int idx = 0;

    for (int x = 0; x < WORLD_SIZE; x++)
        for (int y = 0; y < CHUNK_H; y++)
            for (int z = 0; z < WORLD_SIZE; z++) {
                unsigned char f = face_visible[x][y][z];
                if (!f) continue;
                float bx = (float)x, by = (float)y, bz = (float)(-z);

                #define FACE(cond, v) if(f & cond){ \
                    float d[] = v; \
                    memcpy(&buf[idx], d, 30*sizeof(float)); idx+=30; }

                FACE(FACE_PZ, {
                    bx-0.5f,by-0.5f,bz+0.5f,0,0,
                    bx+0.5f,by-0.5f,bz+0.5f,1,0,
                    bx+0.5f,by+0.5f,bz+0.5f,1,1,
                    bx-0.5f,by-0.5f,bz+0.5f,0,0,
                    bx+0.5f,by+0.5f,bz+0.5f,1,1,
                    bx-0.5f,by+0.5f,bz+0.5f,0,1})

                FACE(FACE_NZ, {
                    bx-0.5f,by-0.5f,bz-0.5f,0,0,
                    bx-0.5f,by+0.5f,bz-0.5f,0,1,
                    bx+0.5f,by+0.5f,bz-0.5f,1,1,
                    bx-0.5f,by-0.5f,bz-0.5f,0,0,
                    bx+0.5f,by+0.5f,bz-0.5f,1,1,
                    bx+0.5f,by-0.5f,bz-0.5f,1,0})

                FACE(FACE_PX, {
                    bx+0.5f,by-0.5f,bz-0.5f,0,0,
                    bx+0.5f,by+0.5f,bz-0.5f,0,1,
                    bx+0.5f,by+0.5f,bz+0.5f,1,1,
                    bx+0.5f,by-0.5f,bz-0.5f,0,0,
                    bx+0.5f,by+0.5f,bz+0.5f,1,1,
                    bx+0.5f,by-0.5f,bz+0.5f,1,0})

                FACE(FACE_NX, {
                    bx-0.5f,by-0.5f,bz-0.5f,0,0,
                    bx-0.5f,by-0.5f,bz+0.5f,1,0,
                    bx-0.5f,by+0.5f,bz+0.5f,1,1,
                    bx-0.5f,by-0.5f,bz-0.5f,0,0,
                    bx-0.5f,by+0.5f,bz+0.5f,1,1,
                    bx-0.5f,by+0.5f,bz-0.5f,0,1})

                FACE(FACE_PY, {
                    bx-0.5f,by+0.5f,bz-0.5f,0,0,
                    bx-0.5f,by+0.5f,bz+0.5f,1,0,
                    bx+0.5f,by+0.5f,bz+0.5f,1,1,
                    bx-0.5f,by+0.5f,bz-0.5f,0,0,
                    bx+0.5f,by+0.5f,bz+0.5f,1,1,
                    bx+0.5f,by+0.5f,bz-0.5f,0,1})

                FACE(FACE_NY, {
                    bx-0.5f,by-0.5f,bz-0.5f,0,0,
                    bx+0.5f,by-0.5f,bz-0.5f,1,0,
                    bx+0.5f,by-0.5f,bz+0.5f,1,1,
                    bx-0.5f,by-0.5f,bz-0.5f,0,0,
                    bx+0.5f,by-0.5f,bz+0.5f,1,1,
                    bx-0.5f,by-0.5f,bz+0.5f,0,1})

                #undef FACE
            }

    if (!eng->vbo) glGenBuffers(1, &eng->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
    glBufferData(GL_ARRAY_BUFFER, floatCount * sizeof(float),
                 buf, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    free(buf);
}

static void render_world(struct engine* eng) {
    glEnable(GL_DEPTH_TEST);
    glUseProgram(eng->program);

    float proj[16], view[16], mvp[16];
    mat4_perspective(proj, GAME_FOV,
                     (float)eng->width / eng->height, 0.1f, 80.0f);
    mat4_lookat(view, eng->camPos, eng->camRot[0], eng->camRot[1]);
    mat4_mul(mvp, proj, view);

    glUniformMatrix4fv(glGetUniformLocation(eng->program, "m"),
                       1, GL_FALSE, mvp);

    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 20, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 20, (void*)12);
    glEnableVertexAttribArray(1);
    glDrawArrays(GL_TRIANGLES, 0, eng->visibleFaceCount * 6);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

#endif
