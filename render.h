#ifndef RENDER_H
#define RENDER_H

#include <GLES2/gl2.h>
#include <stdlib.h>
#include <string.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include "engine.h"
#include "math_utils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define LOG_TAG "Render"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif

void init_textures(struct engine* eng);
void init_ui_shader(void);
void draw_menu(struct engine* eng);
void draw_ui(struct engine* eng);
void render_world(struct engine* eng);

#ifdef __cplusplus
}
#endif

/* ============= ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ============= */
GLuint uiProg = 0;

/* ============= TEXTURES ============= */
static GLuint load_texture(struct android_app* app, const char* filename) {
    AAssetManager* mgr = app->activity->assetManager;
    AAsset* asset = AAssetManager_open(mgr, filename, AASSET_MODE_BUFFER);
    if (!asset) {
        LOGE("Failed to load texture %s", filename);
        return 0;
    }
    size_t len = AAsset_getLength(asset);
    unsigned char* buf = (unsigned char*)malloc(len);
    AAsset_read(asset, buf, len);
    AAsset_close(asset);
    int w, h, ch;
    unsigned char* img = stbi_load_from_memory(buf, (int)len, &w, &h, &ch, 4);
    free(buf);
    if (!img) {
        LOGE("stbi_load failed for %s", filename);
        return 0;
    }
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    stbi_image_free(img);
    return tex;
}

static GLuint make_color_tex(unsigned char r, unsigned char g, unsigned char b) {
    unsigned char px[4] = {r, g, b, 255};
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    return tex;
}

void init_textures(struct engine* eng) {
    eng->texGrassTop = load_texture(eng->app, "grass_top.png");
    eng->texGrassSide = load_texture(eng->app, "grass_side.png");
    eng->texGrassDown = load_texture(eng->app, "grass_down.png");
    if (!eng->texGrassTop) eng->texGrassTop = make_color_tex(100, 180, 60);
    if (!eng->texGrassSide) eng->texGrassSide = make_color_tex(120, 100, 70);
    if (!eng->texGrassDown) eng->texGrassDown = make_color_tex(140, 110, 70);
}

/* ============= ПОСТРОЕНИЕ VBO ============= */
static void push_quad_n(float* buf, int* idx,
    float x0,float y0,float z0,float u0,float v0,
    float x1,float y1,float z1,float u1,float v1,
    float x2,float y2,float z2,float u2,float v2,
    float x3,float y3,float z3,float u3,float v3,
    float nx,float ny,float nz) {
    float vd[6][8] = {
        {x0,y0,z0,u0,v0,nx,ny,nz},
        {x1,y1,z1,u1,v1,nx,ny,nz},
        {x2,y2,z2,u2,v2,nx,ny,nz},
        {x0,y0,z0,u0,v0,nx,ny,nz},
        {x2,y2,z2,u2,v2,nx,ny,nz},
        {x3,y3,z3,u3,v3,nx,ny,nz}};
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
                if(f&FACE_XP)fc++;if(f&FACE_XN)fc++;if(f&FACE_YP)fc++;
                if(f&FACE_YN)fc++;if(f&FACE_ZP)fc++;if(f&FACE_ZN)fc++;
            }
    eng->visibleFaceCount = fc;
    if (fc == 0) {
        eng->meshDirty = false;
        if (eng->vbo) {
            glDeleteBuffers(1, &eng->vbo);
            eng->vbo = 0;
        }
        return;
    }
    int cnt = fc * 6 * 8;
    float* buf = (float*)malloc((size_t)cnt * sizeof(float));
    if (!buf) return;
    int idx = 0;
    int ox = eng->loadCenterX - LOAD_RADIUS;
    int oz = eng->loadCenterZ - LOAD_RADIUS;
    for (int x = 0; x < WORLD_BUF; x++)
        for (int y = 0; y < CHUNK_H; y++)
            for (int z = 0; z < WORLD_BUF; z++) {
                unsigned char f = eng->faces[x][y][z]; if(!f) continue;
                float bx=(float)(ox+x),by=(float)y,bz=(float)(-(oz+z));
                float x0=bx-.5f,x1=bx+.5f,y0=by-.5f,y1=by+.5f,z0=bz-.5f,z1=bz+.5f;
                if(f&FACE_XP)push_quad_n(buf,&idx,x1,y0,z0,1,1,x1,y0,z1,0,1,x1,y1,z1,0,0,x1,y1,z0,1,0,1,0,0);
                if(f&FACE_XN)push_quad_n(buf,&idx,x0,y0,z1,1,1,x0,y0,z0,0,1,x0,y1,z0,0,0,x0,y1,z1,1,0,-1,0,0);
                if(f&FACE_YP)push_quad_n(buf,&idx,x0,y1,z1,1,1,x1,y1,z1,0,1,x1,y1,z0,0,0,x0,y1,z0,1,0,0,1,0);
                if(f&FACE_YN)push_quad_n(buf,&idx,x0,y0,z0,1,1,x1,y0,z0,0,1,x1,y0,z1,0,0,x0,y0,z1,1,0,0,-1,0);
                if(f&FACE_ZP)push_quad_n(buf,&idx,x1,y0,z0,1,1,x0,y0,z0,0,1,x0,y1,z0,0,0,x1,y1,z0,1,0,0,0,-1);
                if(f&FACE_ZN)push_quad_n(buf,&idx,x0,y0,z1,1,1,x1,y0,z1,0,1,x1,y1,z1,0,0,x0,y1,z1,1,0,0,0,1);
            }
    if (!eng->vbo) glGenBuffers(1, &eng->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(cnt*sizeof(float)), buf, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    free(buf);
    eng->meshDirty = false;
}

/* ============= АНИМАЦИЯ БЛОКА (исправлена) ============= */
static void render_anim_block(struct engine* eng) {
    if (!eng->animActive) return;

    float scale = 1.0f;
    float alpha = 1.0f;
    float shakeX = 0, shakeY = 0;

    if (eng->animIsBreak) {
        float t = (float)(ANIM_BREAK_FRAMES - eng->animBreakTimer) / ANIM_BREAK_FRAMES;
        scale = 1.0f - t * 0.3f;
        if (scale < 0.1f) scale = 0.1f;
        alpha = 1.0f - t;
        shakeX = sinf(t * 50.0f) * 0.08f * (1.0f - t);
        shakeY = cosf(t * 45.0f) * 0.05f * (1.0f - t);
        eng->animBreakTimer--;
        if (eng->animBreakTimer <= 0) eng->animActive = false;
    } else {
        float t = (float)(ANIM_PLACE_FRAMES - eng->animPlaceTimer) / ANIM_PLACE_FRAMES;
        scale = 0.3f + t * 0.7f;
        alpha = 0.5f + t * 0.5f;
        float bounce = 1.0f + sinf(t * PI) * 0.15f * (1.0f - t);
        scale *= bounce;
        if (scale > 1.0f) scale = 1.0f;
        eng->animPlaceTimer--;
        if (eng->animPlaceTimer <= 0) {
            eng->animActive = false;
            scale = 1.0f;
            alpha = 1.0f;
        }
    }

    if (scale <= 0.01f) return;

    float bx = eng->animBlockX + shakeX;
    float by = eng->animBlockY + shakeY;
    float bz = eng->animBlockZ;
    float hs = scale * 0.5f;

    float abuf[6 * 48]; 
    int aidx = 0;
    push_quad_n(abuf,&aidx,bx+hs,by-hs,bz-hs,1,1,bx+hs,by-hs,bz+hs,0,1,bx+hs,by+hs,bz+hs,0,0,bx+hs,by+hs,bz-hs,1,0,1,0,0);
    push_quad_n(abuf,&aidx,bx-hs,by-hs,bz+hs,1,1,bx-hs,by-hs,bz-hs,0,1,bx-hs,by+hs,bz-hs,0,0,bx-hs,by+hs,bz+hs,1,0,-1,0,0);
    push_quad_n(abuf,&aidx,bx-hs,by+hs,bz+hs,1,1,bx+hs,by+hs,bz+hs,0,1,bx+hs,by+hs,bz-hs,0,0,bx-hs,by+hs,bz-hs,1,0,0,1,0);
    push_quad_n(abuf,&aidx,bx-hs,by-hs,bz-hs,1,1,bx+hs,by-hs,bz-hs,0,1,bx+hs,by-hs,bz+hs,0,0,bx-hs,by-hs,bz+hs,1,0,0,-1,0);
    push_quad_n(abuf,&aidx,bx+hs,by-hs,bz-hs,1,1,bx-hs,by-hs,bz-hs,0,1,bx-hs,by+hs,bz-hs,0,0,bx+hs,by+hs,bz-hs,1,0,0,0,-1);
    push_quad_n(abuf,&aidx,bx-hs,by-hs,bz+hs,1,1,bx+hs,by-hs,bz+hs,0,1,bx+hs,by+hs,bz+hs,0,0,bx-hs,by+hs,bz+hs,1,0,0,0,1);

    if (alpha < 1.0f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, abuf);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 32, abuf + 3);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 32, abuf + 5);
    glEnableVertexAttribArray(2);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    if (alpha < 1.0f) glDisable(GL_BLEND);
}

/* ============= ОТРИСОВКА МИРА ============= */
void render_world(struct engine* eng) {
    if (eng->meshDirty) rebuild_vbo(eng);
    if (eng->visibleFaceCount == 0) {
        render_anim_block(eng);
        return;
    }
    glEnable(GL_DEPTH_TEST);
    glUseProgram(eng->program);
    glUniform3f(glGetUniformLocation(eng->program, "camPos"),
                eng->camPos[0], eng->camPos[1], eng->camPos[2]);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, eng->texGrassTop);
    glUniform1i(glGetUniformLocation(eng->program, "texTop"), 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, eng->texGrassSide);
    glUniform1i(glGetUniformLocation(eng->program, "texSide"), 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, eng->texGrassDown);
    glUniform1i(glGetUniformLocation(eng->program, "texDown"), 2);

    float proj[16], view[16], mvp[16];
    mat4_perspective(proj, GAME_FOV, (float)eng->width/(float)eng->height, 0.1f, 200.0f);
    mat4_lookat(view, eng->camPos, eng->camRot[0], eng->camRot[1]);
    mat4_mul(mvp, proj, view);
    glUniformMatrix4fv(glGetUniformLocation(eng->program, "m"), 1, GL_FALSE, mvp);

    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,32,(void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,32,(void*)12); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,32,(void*)20); glEnableVertexAttribArray(2);
    glDrawArrays(GL_TRIANGLES, 0, eng->visibleFaceCount * 6);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    render_anim_block(eng);
}

/* ============= UI ШЕЙДЕР ============= */
void init_ui_shader(void) {
    const char* vS = 
        "attribute vec2 aPos;"
        "attribute vec2 aUV;"
        "varying vec2 vUV;"
        "void main(){"
        "  gl_Position=vec4(aPos,0.0,1.0);"
        "  vUV=aUV;"
        "}";
    const char* fS = 
        "precision mediump float;"
        "varying vec2 vUV;"
        "uniform vec4 col;"
        "uniform sampler2D tex;"
        "uniform int useTex;"
        "void main(){"
        "  if(useTex == 1){"
        "    vec4 t=texture2D(tex,vUV);"
        "    gl_FragColor=col*t;"
        "  } else {"
        "    gl_FragColor=col;"
        "  }"
        "}";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vS, NULL);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fS, NULL);
    glCompileShader(fs);
    uiProg = glCreateProgram();
    glAttachShader(uiProg, vs);
    glAttachShader(uiProg, fs);
    glLinkProgram(uiProg);
    glDeleteShader(vs);
    glDeleteShader(fs);
}

/* ============= ВСПОМОГАТЕЛЬНЫЕ UI-ФУНКЦИИ ============= */
static void draw_rect_no_tex(float cx, float cy, float hw, float hh, int sw, int sh,
                              float cr, float cg, float cb, float ca) {
    float nx = (cx/sw)*2.0f-1.0f, ny = 1.0f-(cy/sh)*2.0f;
    float rw = (hw/sw)*2.0f, rh = (hh/sh)*2.0f;
    float v[] = {nx-rw, ny-rh, nx+rw, ny-rh, nx+rw, ny+rh,
                 nx-rw, ny-rh, nx+rw, ny+rh, nx-rw, ny+rh};
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg,"col"), cr,cg,cb,ca);
    glUniform1i(glGetUniformLocation(uiProg, "useTex"), 0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,v);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES,0,6);
}

static void draw_border(float cx, float cy, float hw, float hh, float t, int sw, int sh,
                        float cr, float cg, float cb, float ca) {
    draw_rect_no_tex(cx, cy-hh+t*0.5f, hw, t*0.5f, sw, sh, cr,cg,cb,ca);
    draw_rect_no_tex(cx, cy+hh-t*0.5f, hw, t*0.5f, sw, sh, cr,cg,cb,ca);
    draw_rect_no_tex(cx-hw+t*0.5f, cy, t*0.5f, hh, sw, sh, cr,cg,cb,ca);
    draw_rect_no_tex(cx+hw-t*0.5f, cy, t*0.5f, hh, sw, sh, cr,cg,cb,ca);
}

static void draw_digit(float cx, float cy, float sz, int d, int sw, int sh,
                       float cr, float cg, float cb) {
    float w = sz * 0.4f, h = sz * 0.5f, t = sz * 0.12f;
    bool segs[10][7] = {
        {1,0,1,1,1,1,1}, {0,0,0,0,1,0,1}, {1,1,1,0,1,1,0},
        {1,1,1,0,1,0,1}, {0,1,0,1,1,0,1}, {1,1,1,1,0,0,1},
        {1,1,1,1,0,1,1}, {1,0,0,0,1,0,1}, {1,1,1,1,1,1,1},
        {1,1,1,1,1,0,1}
    };
    if (d < 0 || d > 9) return;
    if (segs[d][0]) draw_rect_no_tex(cx, cy-h, w, t, sw, sh, cr,cg,cb,1);
    if (segs[d][1]) draw_rect_no_tex(cx, cy, w, t, sw, sh, cr,cg,cb,1);
    if (segs[d][2]) draw_rect_no_tex(cx, cy+h, w, t, sw, sh, cr,cg,cb,1);
    if (segs[d][3]) draw_rect_no_tex(cx-w, cy-h*0.5f, t, h*0.5f+t, sw, sh, cr,cg,cb,1);
    if (segs[d][4]) draw_rect_no_tex(cx+w, cy-h*0.5f, t, h*0.5f+t, sw, sh, cr,cg,cb,1);
    if (segs[d][5]) draw_rect_no_tex(cx-w, cy+h*0.5f, t, h*0.5f+t, sw, sh, cr,cg,cb,1);
    if (segs[d][6]) draw_rect_no_tex(cx+w, cy+h*0.5f, t, h*0.5f+t, sw, sh, cr,cg,cb,1);
}

static void draw_ring(float cx, float cy, float r, float thick, int w, int h,
                      float cr, float cg, float cb, float ca) {
    float ndcX = (cx/w)*2-1, ndcY = 1-(cy/h)*2;
    float rxo = (r/w)*2, ryo = (r/h)*2;
    float rxi = ((r-thick)/w)*2, ryi = ((r-thick)/h)*2;
    float verts[(32+1)*4];
    int segs = 32;
    for(int i=0;i<=segs;i++){
        float a = (float)i/segs*2*PI;
        float c = cosf(a), s = sinf(a);
        verts[i*4] = ndcX + c*rxo;
        verts[i*4+1] = ndcY + s*ryo;
        verts[i*4+2] = ndcX + c*rxi;
        verts[i*4+3] = ndcY + s*ryi;
    }
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg,"col"), cr,cg,cb,ca);
    glUniform1i(glGetUniformLocation(uiProg, "useTex"), 0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_STRIP,0,(segs+1)*2);
}

static void draw_circle(float cx, float cy, float r, int w, int h,
                        float cr, float cg, float cb, float ca) {
    float ndcX = (cx/w)*2-1, ndcY = 1-(cy/h)*2;
    float rx = (r/w)*2, ry = (r/h)*2;
    float verts[(24+2)*2];
    int segs = 24;
    verts[0] = ndcX; verts[1] = ndcY;
    for(int i=0;i<=segs;i++){
        float a = (float)i/segs*2*PI;
        verts[(i+1)*2] = ndcX + cosf(a)*rx;
        verts[(i+1)*2+1] = ndcY + sinf(a)*ry;
    }
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg,"col"), cr,cg,cb,ca);
    glUniform1i(glGetUniformLocation(uiProg, "useTex"), 0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_FAN,0,segs+2);
}

/* ============= МЕНЮ (только кнопка Play) ============= */
void draw_menu(struct engine* eng) {
    int sw = eng->width, sh = eng->height;
    
    // Фон
    draw_rect_no_tex(sw/2.0f, sh/2.0f, sw/2.0f, sh/2.0f, sw, sh, 0.2f, 0.6f, 0.3f, 1.0f);
    
    // Название
    float titleY = sh * 0.3f;
    draw_rect_no_tex(sw/2.0f, titleY, 150, 40, sw, sh, 0, 0, 0, 0.4f);
    
    // Кнопка PLAY
    float playX = sw/2.0f, playY = sh * 0.55f;
    draw_rect_no_tex(playX, playY, 80, 30, sw, sh, 0.15f, 0.5f, 0.15f, 0.9f);
    draw_border(playX, playY, 80, 30, 3, sw, sh, 0.3f, 1.0f, 0.3f, 1.0f);
    
    // Треугольник Play
    float pnx = (playX/sw)*2-1, pny = 1-(playY/sh)*2;
    float pax = (20.0f/sw)*2, pay = (20.0f/sh)*2;
    float play[] = {pnx-pax, pny+pay, pnx-pax, pny-pay, pnx+pax, pny};
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg,"col"), 1.0f, 1.0f, 1.0f, 1.0f);
    glUniform1i(glGetUniformLocation(uiProg, "useTex"), 0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, play);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

/* ============= UI ИГРЫ (инвентарь с правильными UV) ============= */
static void render_inv_block_2d(struct engine* eng, float screenX, float screenY, float size) {
    float ndcX = (screenX / eng->width) * 2.0f - 1.0f;
    float ndcY = 1.0f - (screenY / eng->height) * 2.0f;
    float s = size / eng->height * 2.0f;
    
    // Правильные UV координаты для квадрата
    float verts[] = {
        ndcX - s, ndcY - s, 0.0f, 0.0f,
        ndcX + s, ndcY - s, 1.0f, 0.0f,
        ndcX + s, ndcY + s, 1.0f, 1.0f,
        ndcX - s, ndcY - s, 0.0f, 0.0f,
        ndcX + s, ndcY + s, 1.0f, 1.0f,
        ndcX - s, ndcY + s, 0.0f, 1.0f
    };
    
    glUseProgram(uiProg);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, eng->texGrassSide);
    glUniform1i(glGetUniformLocation(uiProg, "tex"), 0);
    glUniform4f(glGetUniformLocation(uiProg, "col"), 1.0f, 1.0f, 1.0f, 1.0f);
    glUniform1i(glGetUniformLocation(uiProg, "useTex"), 1);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, verts);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, verts + 2);
    glEnableVertexAttribArray(1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void draw_ui(struct engine* eng) {
    int sw = eng->width, sh = eng->height;
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (eng->gameState == STATE_MENU) {
        draw_menu(eng);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        return;
    }

    // Прицел
    float cx = (float)(sw / 2), cy = (float)(sh / 2);
    draw_rect_no_tex(cx, cy, 10, 1, sw, sh, 1,1,1,1);
    draw_rect_no_tex(cx, cy, 1, 10, sw, sh, 1,1,1,1);

    // Джойстик
    float jx = JOY_X_OFFSET, jy = sh - JOY_Y_OFFSET;
    draw_ring(jx, jy, JOY_RADIUS, 3, sw, sh, 0,0,0,1);
    draw_circle(jx + eng->moveDirX*JOY_RADIUS*0.6f,
                jy + eng->moveDirZ*JOY_RADIUS*0.6f,
                STICK_RADIUS, sw, sh, 0,0,0,1);

    // Прыжок
    float bx = sw - JUMP_BTN_OFFSET, by = sh - JUMP_BTN_OFFSET;
    draw_ring(bx, by, JUMP_BTN_SIZE, 3, sw, sh, 0,0,0,1);
    float as = JUMP_BTN_SIZE*0.3f;
    float anx=(bx/sw)*2-1, any=1-(by/sh)*2;
    float aax=(as/sw)*2, aay=(as/sh)*2;
    float arrow[] = {anx, any+aay, anx-aax, any-aay*0.5f, anx+aax, any-aay*0.5f};
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg,"col"),0,0,0,1);
    glUniform1i(glGetUniformLocation(uiProg, "useTex"), 0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,arrow);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES,0,3);

    // Ломание
    float bbx = sw-BREAK_BTN_X, bby = BREAK_BTN_Y;
    draw_ring(bbx,bby,ACTION_BTN_SIZE,3,sw,sh,0,0,0,1);
    float xsz=ACTION_BTN_SIZE*0.22f,xw=2.5f;
    float xnx=(bbx/sw)*2-1, xny=1-(bby/sh)*2;
    float xs=(xsz/sw)*2, ys=(xsz/sh)*2, xwn=(xw/sw)*2;
    float d1[] = {xnx-xs-xwn,xny+ys, xnx-xs+xwn,xny+ys, xnx+xs+xwn,xny-ys,
                  xnx-xs-xwn,xny+ys, xnx+xs+xwn,xny-ys, xnx+xs-xwn,xny-ys};
    glUniform4f(glGetUniformLocation(uiProg,"col"),0,0,0,1);
    glUniform1i(glGetUniformLocation(uiProg, "useTex"), 0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,d1);
    glDrawArrays(GL_TRIANGLES,0,6);
    float d2[] = {xnx+xs-xwn,xny+ys, xnx+xs+xwn,xny+ys, xnx-xs+xwn,xny-ys,
                  xnx+xs-xwn,xny+ys, xnx-xs+xwn,xny-ys, xnx-xs-xwn,xny-ys};
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,d2);
    glDrawArrays(GL_TRIANGLES,0,6);

    // Ставление
    float pbx = sw-PLACE_BTN_X, pby = PLACE_BTN_Y;
    draw_ring(pbx,pby,ACTION_BTN_SIZE,3,sw,sh,0,0,0,1);
    draw_rect_no_tex(pbx,pby,ACTION_BTN_SIZE*0.3f,2.5f,sw,sh,0,0,0,1);
    draw_rect_no_tex(pbx,pby,2.5f,ACTION_BTN_SIZE*0.3f,sw,sh,0,0,0,1);

    // Инвентарь
    float invW = INV_SLOTS*(INV_SLOT_SIZE+INV_PADDING)-INV_PADDING;
    float invStartX = (sw-invW)/2.0f;
    float invY = sh-INV_Y_OFFSET;
    float hs = INV_SLOT_SIZE/2.0f;

    for (int i = 0; i < INV_SLOTS; i++) {
        float slotX = invStartX + i*(INV_SLOT_SIZE+INV_PADDING) + hs;
        bool sel = (i == eng->selectedSlot);
        if (sel) draw_rect_no_tex(slotX, invY, hs, hs, sw, sh, 0.85f,0.85f,0.85f, 0.9f);
        else draw_rect_no_tex(slotX, invY, hs, hs, sw, sh, 0.12f,0.12f,0.12f, 0.75f);
        draw_border(slotX, invY, hs, hs, 2.0f, sw, sh,
                    sel?1.0f:0.35f, sel?1.0f:0.35f, sel?1.0f:0.35f, 1.0f);
        if (eng->invSlots[i] != BLOCK_AIR) {
            render_inv_block_2d(eng, slotX, invY, hs*0.6f);
        }
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

#endif
