#ifndef RENDER_H
#define RENDER_H

#include <GLES2/gl2.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "world.h"
#include "math_utils.h"

static void push_quad(float* buf, int* idx,
                      float x0, float y0, float z0, float u0, float v0,
                      float x1, float y1, float z1, float u1, float v1,
                      float x2, float y2, float z2, float u2, float v2,
                      float x3, float y3, float z3, float u3, float v3) {
    buf[(*idx)++] = x0; buf[(*idx)++] = y0; buf[(*idx)++] = z0;
    buf[(*idx)++] = u0; buf[(*idx)++] = v0;
    buf[(*idx)++] = x1; buf[(*idx)++] = y1; buf[(*idx)++] = z1;
    buf[(*idx)++] = u1; buf[(*idx)++] = v1;
    buf[(*idx)++] = x2; buf[(*idx)++] = y2; buf[(*idx)++] = z2;
    buf[(*idx)++] = u2; buf[(*idx)++] = v2;
    buf[(*idx)++] = x0; buf[(*idx)++] = y0; buf[(*idx)++] = z0;
    buf[(*idx)++] = u0; buf[(*idx)++] = v0;
    buf[(*idx)++] = x2; buf[(*idx)++] = y2; buf[(*idx)++] = z2;
    buf[(*idx)++] = u2; buf[(*idx)++] = v2;
    buf[(*idx)++] = x3; buf[(*idx)++] = y3; buf[(*idx)++] = z3;
    buf[(*idx)++] = u3; buf[(*idx)++] = v3;
}

static int count_faces(void) {
    int c = 0;
    for (int x = 0; x < WORLD_SIZE; x++)
        for (int y = 0; y < CHUNK_H; y++)
            for (int z = 0; z < WORLD_SIZE; z++) {
                unsigned char f = face_vis[x][y][z];
                if (f & FACE_XP) c++;
                if (f & FACE_XN) c++;
                if (f & FACE_YP) c++;
                if (f & FACE_YN) c++;
                if (f & FACE_ZP) c++;
                if (f & FACE_ZN) c++;
            }
    return c;
}

static void rebuild_world_vbo(struct engine* eng) {
    int faceCount = count_faces();
    eng->visibleFaceCount = faceCount;
    if (faceCount == 0) return;

    int floatCount = faceCount * 6 * 5;
    float* buf = (float*)malloc((size_t)floatCount * sizeof(float));
    if (!buf) return;
    int idx = 0;

    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int y = 0; y < CHUNK_H; y++) {
            for (int z = 0; z < WORLD_SIZE; z++) {
                unsigned char f = face_vis[x][y][z];
                if (!f) continue;

                float bx = (float)x;
                float by = (float)y;
                float bz = (float)(-z);

                float x0 = bx - 0.5f, x1 = bx + 0.5f;
                float y0 = by - 0.5f, y1 = by + 0.5f;
                float z0 = bz - 0.5f, z1 = bz + 0.5f;

                if (f & FACE_XP)
                    push_quad(buf, &idx,
                        x1,y0,z0, 0,0, x1,y0,z1, 1,0,
                        x1,y1,z1, 1,1, x1,y1,z0, 0,1);
                if (f & FACE_XN)
                    push_quad(buf, &idx,
                        x0,y0,z1, 0,0, x0,y0,z0, 1,0,
                        x0,y1,z0, 1,1, x0,y1,z1, 0,1);
                if (f & FACE_YP)
                    push_quad(buf, &idx,
                        x0,y1,z1, 0,0, x1,y1,z1, 1,0,
                        x1,y1,z0, 1,1, x0,y1,z0, 0,1);
                if (f & FACE_YN)
                    push_quad(buf, &idx,
                        x0,y0,z0, 0,0, x1,y0,z0, 1,0,
                        x1,y0,z1, 1,1, x0,y0,z1, 0,1);
                if (f & FACE_ZP)
                    push_quad(buf, &idx,
                        x1,y0,z0, 0,0, x0,y0,z0, 1,0,
                        x0,y1,z0, 1,1, x1,y1,z0, 0,1);
                if (f & FACE_ZN)
                    push_quad(buf, &idx,
                        x0,y0,z1, 0,0, x1,y0,z1, 1,0,
                        x1,y1,z1, 1,1, x0,y1,z1, 0,1);
            }
        }
    }

    if (!eng->vbo) glGenBuffers(1, &eng->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(floatCount * sizeof(float)),
                 buf, GL_STATIC_DRAW);
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

/* ============= UI — непрозрачный ============= */

static GLuint uiProg = 0;

static void init_ui_shader(void) {
    const char* vS = "attribute vec4 p; void main(){ gl_Position=p; }";
    const char* fS =
        "precision mediump float; uniform vec4 col;"
        "void main(){ gl_FragColor=col; }";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vS, NULL); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fS, NULL); glCompileShader(fs);
    uiProg = glCreateProgram();
    glAttachShader(uiProg, vs);
    glAttachShader(uiProg, fs);
    glLinkProgram(uiProg);
    glDeleteShader(vs);
    glDeleteShader(fs);
}

static void draw_ring(float cx, float cy, float r, float thick,
                      int w, int h,
                      float cr, float cg, float cb, float ca) {
    float ndcX = (cx / w) * 2.0f - 1.0f;
    float ndcY = 1.0f - (cy / h) * 2.0f;
    float rx_o = (r / w) * 2.0f, ry_o = (r / h) * 2.0f;
    float rx_i = ((r - thick) / w) * 2.0f, ry_i = ((r - thick) / h) * 2.0f;
    int segs = 32;
    float verts[(32 + 1) * 4];
    for (int i = 0; i <= segs; i++) {
        float a = (float)i / segs * 2.0f * PI;
        float cs = cosf(a), sn = sinf(a);
        verts[i * 4 + 0] = ndcX + cs * rx_o;
        verts[i * 4 + 1] = ndcY + sn * ry_o;
        verts[i * 4 + 2] = ndcX + cs * rx_i;
        verts[i * 4 + 3] = ndcY + sn * ry_i;
    }
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg, "col"), cr, cg, cb, ca);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, (segs + 1) * 2);
}

static void draw_circle(float cx, float cy, float r,
                         int w, int h,
                         float cr, float cg, float cb, float ca) {
    float ndcX = (cx / w) * 2.0f - 1.0f;
    float ndcY = 1.0f - (cy / h) * 2.0f;
    float rx = (r / w) * 2.0f, ry = (r / h) * 2.0f;
    int segs = 24;
    float verts[(24 + 2) * 2];
    verts[0] = ndcX;
    verts[1] = ndcY;
    for (int i = 0; i <= segs; i++) {
        float a = (float)i / segs * 2.0f * PI;
        verts[(i + 1) * 2 + 0] = ndcX + cosf(a) * rx;
        verts[(i + 1) * 2 + 1] = ndcY + sinf(a) * ry;
    }
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg, "col"), cr, cg, cb, ca);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, segs + 2);
}

static void draw_ui(struct engine* eng) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float jx = JOY_X_OFFSET;
    float jy = eng->height - JOY_Y_OFFSET;

    /* Заливка джойстика — тёмно-серая непрозрачная */
    draw_circle(jx, jy, JOY_RADIUS,
                eng->width, eng->height,
                0.15f, 0.15f, 0.15f, 1.0f);

    /* Обводка джойстика — белая */
    draw_ring(jx, jy, JOY_RADIUS, 4.0f,
              eng->width, eng->height,
              1.0f, 1.0f, 1.0f, 1.0f);

    /* Стик — светло-серый непрозрачный */
    float hx = jx + eng->moveDirX * JOY_RADIUS * 0.6f;
    float hy = jy + eng->moveDirZ * JOY_RADIUS * 0.6f;
    draw_circle(hx, hy, STICK_RADIUS,
                eng->width, eng->height,
                0.6f, 0.6f, 0.6f, 1.0f);

    /* Обводка стика */
    draw_ring(hx, hy, STICK_RADIUS, 3.0f,
              eng->width, eng->height,
              1.0f, 1.0f, 1.0f, 1.0f);

    /* Кнопка прыжка — заливка */
    float bx = eng->width - JUMP_BTN_OFFSET;
    float by = eng->height - JUMP_BTN_OFFSET;
    draw_circle(bx, by, JUMP_BTN_SIZE,
                eng->width, eng->height,
                0.15f, 0.15f, 0.15f, 1.0f);

    /* Обводка прыжка */
    draw_ring(bx, by, JUMP_BTN_SIZE, 4.0f,
              eng->width, eng->height,
              1.0f, 1.0f, 1.0f, 1.0f);

    /* Стрелка вверх — белая */
    float as = JUMP_BTN_SIZE * 0.35f;
    float nx = (bx / eng->width) * 2.0f - 1.0f;
    float ny = 1.0f - (by / eng->height) * 2.0f;
    float ax = (as / eng->width) * 2.0f;
    float ay = (as / eng->height) * 2.0f;
    float arrow[] = {
        nx,       ny + ay,
        nx - ax,  ny - ay * 0.5f,
        nx + ax,  ny - ay * 0.5f
    };
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg, "col"),
                1.0f, 1.0f, 1.0f, 1.0f);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, arrow);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

#endif
