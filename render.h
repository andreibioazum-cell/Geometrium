#ifndef RENDER_H
#define RENDER_H

#include <GLES2/gl2.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "world.h"
#include "math_utils.h"

static void push_face(float* buf, int* idx, const float* v) {
    memcpy(&buf[*idx], v, 30 * sizeof(float));
    *idx += 30;
}

static int count_visible_faces(unsigned char f) {
    int c = 0;
    if (f & FACE_PX) c++;
    if (f & FACE_NX) c++;
    if (f & FACE_PY) c++;
    if (f & FACE_NY) c++;
    if (f & FACE_PZ) c++;
    if (f & FACE_NZ) c++;
    return c;
}

static void rebuild_world_vbo(struct engine* eng) {
    int faceCount = 0;

    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int y = 0; y < CHUNK_H; y++) {
            for (int z = 0; z < WORLD_SIZE; z++) {
                faceCount += count_visible_faces(face_visible[x][y][z]);
            }
        }
    }

    eng->visibleFaceCount = faceCount;
    if (faceCount == 0) return;

    int floatCount = faceCount * 6 * 5;
    float* buf = (float*)malloc((size_t)floatCount * sizeof(float));
    if (!buf) return;

    int idx = 0;

    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int y = 0; y < CHUNK_H; y++) {
            for (int z = 0; z < WORLD_SIZE; z++) {
                unsigned char f = face_visible[x][y][z];
                if (!f) continue;

                float bx = (float)x;
                float by = (float)y;
                float bz = (float)(-z);

                if (f & FACE_PZ) {
                    const float v[30] = {
                        bx-0.5f, by-0.5f, bz+0.5f, 0,0,
                        bx+0.5f, by-0.5f, bz+0.5f, 1,0,
                        bx+0.5f, by+0.5f, bz+0.5f, 1,1,
                        bx-0.5f, by-0.5f, bz+0.5f, 0,0,
                        bx+0.5f, by+0.5f, bz+0.5f, 1,1,
                        bx-0.5f, by+0.5f, bz+0.5f, 0,1
                    };
                    push_face(buf, &idx, v);
                }

                if (f & FACE_NZ) {
                    const float v[30] = {
                        bx-0.5f, by-0.5f, bz-0.5f, 0,0,
                        bx-0.5f, by+0.5f, bz-0.5f, 0,1,
                        bx+0.5f, by+0.5f, bz-0.5f, 1,1,
                        bx-0.5f, by-0.5f, bz-0.5f, 0,0,
                        bx+0.5f, by+0.5f, bz-0.5f, 1,1,
                        bx+0.5f, by-0.5f, bz-0.5f, 1,0
                    };
                    push_face(buf, &idx, v);
                }

                if (f & FACE_PX) {
                    const float v[30] = {
                        bx+0.5f, by-0.5f, bz-0.5f, 0,0,
                        bx+0.5f, by+0.5f, bz-0.5f, 0,1,
                        bx+0.5f, by+0.5f, bz+0.5f, 1,1,
                        bx+0.5f, by-0.5f, bz-0.5f, 0,0,
                        bx+0.5f, by+0.5f, bz+0.5f, 1,1,
                        bx+0.5f, by-0.5f, bz+0.5f, 1,0
                    };
                    push_face(buf, &idx, v);
                }

                if (f & FACE_NX) {
                    const float v[30] = {
                        bx-0.5f, by-0.5f, bz-0.5f, 0,0,
                        bx-0.5f, by-0.5f, bz+0.5f, 1,0,
                        bx-0.5f, by+0.5f, bz+0.5f, 1,1,
                        bx-0.5f, by-0.5f, bz-0.5f, 0,0,
                        bx-0.5f, by+0.5f, bz+0.5f, 1,1,
                        bx-0.5f, by+0.5f, bz-0.5f, 0,1
                    };
                    push_face(buf, &idx, v);
                }

                if (f & FACE_PY) {
                    const float v[30] = {
                        bx-0.5f, by+0.5f, bz-0.5f, 0,0,
                        bx-0.5f, by+0.5f, bz+0.5f, 1,0,
                        bx+0.5f, by+0.5f, bz+0.5f, 1,1,
                        bx-0.5f, by+0.5f, bz-0.5f, 0,0,
                        bx+0.5f, by+0.5f, bz+0.5f, 1,1,
                        bx+0.5f, by+0.5f, bz-0.5f, 0,1
                    };
                    push_face(buf, &idx, v);
                }

                if (f & FACE_NY) {
                    const float v[30] = {
                        bx-0.5f, by-0.5f, bz-0.5f, 0,0,
                        bx+0.5f, by-0.5f, bz-0.5f, 1,0,
                        bx+0.5f, by-0.5f, bz+0.5f, 1,1,
                        bx-0.5f, by-0.5f, bz-0.5f, 0,0,
                        bx+0.5f, by-0.5f, bz+0.5f, 1,1,
                        bx-0.5f, by-0.5f, bz+0.5f, 0,1
                    };
                    push_face(buf, &idx, v);
                }
            }
        }
    }

    if (!eng->vbo) glGenBuffers(1, &eng->vbo);

    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(floatCount * sizeof(float)),
                 buf,
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    free(buf);
}

static void render_world(struct engine* eng) {
    glEnable(GL_DEPTH_TEST);
    glUseProgram(eng->program);

    float proj[16], view[16], mvp[16];
    mat4_perspective(proj, GAME_FOV,
                     (float)eng->width / (float)eng->height,
                     0.1f, 80.0f);
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
