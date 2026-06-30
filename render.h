#ifndef RENDER_H
#define RENDER_H

#include <GLES2/gl2.h>
#include <stdlib.h>
#include <string.h>
#include <android/asset_manager.h>
#include "engine.h"
#include "math_utils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static GLuint load_texture(struct android_app* app, const char* filename) {
    AAssetManager* mgr = app->activity->assetManager;
    AAsset* asset = AAssetManager_open(mgr, filename, AASSET_MODE_BUFFER);
    if (!asset) return 0;
    size_t len = AAsset_getLength(asset);
    unsigned char* buf = (unsigned char*)malloc(len);
    AAsset_read(asset, buf, len);
    AAsset_close(asset);
    int w, h, ch;
    unsigned char* img = stbi_load_from_memory(buf, (int)len, &w, &h, &ch, 4);
    free(buf);
    if (!img) return 0;
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, img);
    stbi_image_free(img);
    return tex;
}

static void push_quad_n(float* buf, int* idx,
    float x0, float y0, float z0, float u0, float v0,
    float x1, float y1, float z1, float u1, float v1,
    float x2, float y2, float z2, float u2, float v2,
    float x3, float y3, float z3, float u3, float v3,
    float nx, float ny, float nz) {
    float vd[6][8] = {
        {x0,y0,z0,u0,v0,nx,ny,nz}, {x1,y1,z1,u1,v1,nx,ny,nz},
        {x2,y2,z2,u2,v2,nx,ny,nz}, {x0,y0,z0,u0,v0,nx,ny,nz},
        {x2,y2,z2,u2,v2,nx,ny,nz}, {x3,y3,z3,u3,v3,nx,ny,nz}
    };
    memcpy(&buf[*idx], vd, sizeof(vd));
    *idx += 48;
}

static void rebuild_vbo(struct engine* eng) {
    update_faces(eng);

    int fc = 0;
    for (int x = 0; x < WORLD_BUF; x++)
        for (int y = 0; y < CHUNK_H; y++)
            for (int z = 0; z < WORLD_BUF; z++) {
                unsigned char f = eng->faces[x][y][z];
                if (f & FACE_XP) fc++;
                if (f & FACE_XN) fc++;
                if (f & FACE_YP) fc++;
                if (f & FACE_YN) fc++;
                if (f & FACE_ZP) fc++;
                if (f & FACE_ZN) fc++;
            }

    eng->visibleFaceCount = fc;
    if (fc == 0) { eng->meshDirty = false; return; }

    int cnt = fc * 6 * 8;
    float* buf = (float*)malloc((size_t)cnt * sizeof(float));
    if (!buf) return;
    int idx = 0;

    int ox = eng->loadCenterX - LOAD_RADIUS;
    int oz = eng->loadCenterZ - LOAD_RADIUS;

    for (int x = 0; x < WORLD_BUF; x++)
        for (int y = 0; y < CHUNK_H; y++)
            for (int z = 0; z < WORLD_BUF; z++) {
                unsigned char f = eng->faces[x][y][z];
                if (!f) continue;
                float bx = (float)(ox + x);
                float by = (float)y;
                float bz = (float)(-(oz + z));
                float x0 = bx - 0.5f, x1 = bx + 0.5f;
                float y0 = by - 0.5f, y1 = by + 0.5f;
                float z0 = bz - 0.5f, z1 = bz + 0.5f;

                if (f & FACE_XP)
                    push_quad_n(buf, &idx,
                        x1,y0,z0,0,1, x1,y0,z1,1,1,
                        x1,y1,z1,1,0, x1,y1,z0,0,0, 1,0,0);
                if (f & FACE_XN)
                    push_quad_n(buf, &idx,
                        x0,y0,z1,0,1, x0,y0,z0,1,1,
                        x0,y1,z0,1,0, x0,y1,z1,0,0, -1,0,0);
                if (f & FACE_YP)
                    push_quad_n(buf, &idx,
                        x0,y1,z1,0,0, x1,y1,z1,1,0,
                        x1,y1,z0,1,1, x0,y1,z0,0,1, 0,1,0);
                if (f & FACE_YN)
                    push_quad_n(buf, &idx,
                        x0,y0,z0,0,0, x1,y0,z0,1,0,
                        x1,y0,z1,1,1, x0,y0,z1,0,1, 0,-1,0);
                if (f & FACE_ZP)
                    push_quad_n(buf, &idx,
                        x1,y0,z0,0,1, x0,y0,z0,1,1,
                        x0,y1,z0,1,0, x1,y1,z0,0,0, 0,0,-1);
                if (f & FACE_ZN)
                    push_quad_n(buf, &idx,
                        x0,y0,z1,0,1, x1,y0,z1,1,1,
                        x1,y1,z1,1,0, x0,y1,z1,0,0, 0,0,1);
            }

    if (!eng->vbo) glGenBuffers(1, &eng->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(cnt * sizeof(float)),
                 buf, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    free(buf);
    eng->meshDirty = false;
}

static void render_world(struct engine* eng) {
    if (eng->meshDirty) rebuild_vbo(eng);
    if (eng->visibleFaceCount == 0) return;

    glEnable(GL_DEPTH_TEST);
    glUseProgram(eng->program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, eng->texture);
    glUniform1i(glGetUniformLocation(eng->program, "tex"), 0);
    glUniform3f(glGetUniformLocation(eng->program, "camPos"),
                eng->camPos[0], eng->camPos[1], eng->camPos[2]);

    float proj[16], view[16], mvp[16];
    mat4_perspective(proj, GAME_FOV,
                     (float)eng->width / (float)eng->height,
                     0.1f, 120.0f);
    mat4_lookat(view, eng->camPos, eng->camRot[0], eng->camRot[1]);
    mat4_mul(mvp, proj, view);
    glUniformMatrix4fv(glGetUniformLocation(eng->program, "m"),
                       1, GL_FALSE, mvp);

    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 32, (void*)12);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 32, (void*)20);
    glEnableVertexAttribArray(2);
    glDrawArrays(GL_TRIANGLES, 0, eng->visibleFaceCount * 6);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/* ============= UI ============= */
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

static void draw_rect(float cx, float cy, float hw, float hh,
                      int sw, int sh,
                      float cr, float cg, float cb, float ca) {
    float nx = (cx / sw) * 2.0f - 1.0f;
    float ny = 1.0f - (cy / sh) * 2.0f;
    float rw = (hw / sw) * 2.0f;
    float rh = (hh / sh) * 2.0f;
    float verts[] = {
        nx - rw, ny - rh, nx + rw, ny - rh, nx + rw, ny + rh,
        nx - rw, ny - rh, nx + rw, ny + rh, nx - rw, ny + rh
    };
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg, "col"), cr, cg, cb, ca);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static void draw_ui(struct engine* eng) {
    int sw = eng->width;
    int sh = eng->height;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Прицел — чёрный крестик с белой обводкой для видимости на любом фоне */
    float ccx = sw / 2.0f;
    float ccy = sh / 2.0f;
    /* Белая обводка */
    draw_rect(ccx, ccy, 12.0f, 2.5f, sw, sh, 1, 1, 1, 0.8f);
    draw_rect(ccx, ccy, 2.5f, 12.0f, sw, sh, 1, 1, 1, 0.8f);
    /* Чёрный центр */
    draw_rect(ccx, ccy, 10.0f, 1.5f, sw, sh, 0, 0, 0, 1.0f);
    draw_rect(ccx, ccy, 1.5f, 10.0f, sw, sh, 0, 0, 0, 1.0f);

    /* Джойстик */
    float jx = JOY_X_OFFSET;
    float jy = sh - JOY_Y_OFFSET;
    draw_ring(jx, jy, JOY_RADIUS, 3.0f, sw, sh, 0, 0, 0, 1.0f);
    float hx = jx + eng->moveDirX * JOY_RADIUS * 0.6f;
    float hy = jy + eng->moveDirZ * JOY_RADIUS * 0.6f;
    draw_circle(hx, hy, STICK_RADIUS, sw, sh, 0, 0, 0, 1.0f);

    /* Прыжок */
    float bx = sw - JUMP_BTN_OFFSET;
    float by = sh - JUMP_BTN_OFFSET;
    draw_ring(bx, by, JUMP_BTN_SIZE, 3.0f, sw, sh, 0, 0, 0, 1.0f);
    float as = JUMP_BTN_SIZE * 0.3f;
    float anx = (bx / sw) * 2.0f - 1.0f;
    float any = 1.0f - (by / sh) * 2.0f;
    float aax = (as / sw) * 2.0f;
    float aay = (as / sh) * 2.0f;
    float arrow[] = {
        anx, any + aay,
        anx - aax, any - aay * 0.5f,
        anx + aax, any - aay * 0.5f
    };
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg, "col"), 0, 0, 0, 1.0f);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, arrow);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    /* Кнопка ломания — X */
    float bbx = sw - BREAK_BTN_X;
    float bby = BREAK_BTN_Y;
    draw_ring(bbx, bby, ACTION_BTN_SIZE, 3.0f, sw, sh, 0, 0, 0, 1.0f);
    /* X из двух перекрёстных полосок */
    float xsz = ACTION_BTN_SIZE * 0.28f;
    float xw = 2.5f;
    /* Полоска \ */
    draw_rect(bbx - xsz * 0.5f, bby - xsz * 0.5f, xw, xsz * 0.15f, sw, sh, 0, 0, 0, 1.0f);
    draw_rect(bbx + xsz * 0.5f, bby + xsz * 0.5f, xw, xsz * 0.15f, sw, sh, 0, 0, 0, 1.0f);
    /* Просто два прямоугольника крестом */
    draw_rect(bbx, bby, xsz, xw, sw, sh, 0, 0, 0, 1.0f);
    draw_rect(bbx, bby, xw, xsz, sw, sh, 0, 0, 0, 1.0f);

    /* Кнопка ставления — + */
    float pbx = sw - PLACE_BTN_X;
    float pby = PLACE_BTN_Y;
    draw_ring(pbx, pby, ACTION_BTN_SIZE, 3.0f, sw, sh, 0, 0, 0, 1.0f);
    float psz = ACTION_BTN_SIZE * 0.3f;
    float pw = 2.5f;
    draw_rect(pbx, pby, psz, pw, sw, sh, 0, 0, 0, 1.0f);
    draw_rect(pbx, pby, pw, psz, sw, sh, 0, 0, 0, 1.0f);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

#endif
