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

static GLuint uiProg = 0;

static GLuint load_tex(struct android_app* app, const char* name) {
    AAssetManager* mgr = app->activity->assetManager;
    AAsset* asset = AAssetManager_open(mgr, name, AASSET_MODE_BUFFER);
    if (!asset) return 0;
    size_t len = AAsset_getLength(asset);
    unsigned char* buf = (unsigned char*)malloc(len);
    AAsset_read(asset, buf, len); AAsset_close(asset);
    int w, h, ch;
    unsigned char* img = stbi_load_from_memory(buf, (int)len, &w, &h, &ch, 4);
    free(buf); if (!img) return 0;
    GLuint tex; glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    stbi_image_free(img); return tex;
}

void init_textures(struct engine* eng) {
    eng->texGrassTop = load_tex(eng->app, "grass_top.png");
    eng->texGrassSide = load_tex(eng->app, "grass_side.png");
    eng->texGrassDown = load_tex(eng->app, "grass_down.png");
    eng->texLeaves = load_tex(eng->app, "foliage.png");
    eng->texTreeSide = load_tex(eng->app, "tree_side.png");
    eng->texTreeTop = load_tex(eng->app, "tree_top_down.png");
}

static void p_v(float* b, int* i, float x, float y, float z, float u, float v, float nx, float ny, float nz, float t) {
    float d[] = {x,y,z, u,v, nx,ny,nz, t};
    memcpy(&b[*i], d, sizeof(d));
    *i += 9;
}

static void rebuild_vbo(struct engine* eng) {
    update_faces(eng);
    int fc = 0;
    for (int x=0; x<WORLD_BUF; x++) for (int y=0; y<CHUNK_H; y++) for (int z=0; z<WORLD_BUF; z++) {
        unsigned char f = eng->faces[x][y][z];
        if(f&FACE_XP)fc++; if(f&FACE_XN)fc++; if(f&FACE_YP)fc++; if(f&FACE_YN)fc++; if(f&FACE_ZP)fc++; if(f&FACE_ZN)fc++;
    }
    eng->visibleFaceCount = fc;
    if (fc == 0) return;
    float* buf = (float*)malloc(fc * 6 * 9 * sizeof(float));
    if(!buf) return;
    int idx = 0, ox = eng->loadCenterX - LOAD_RADIUS, oz = eng->loadCenterZ - LOAD_RADIUS;
    for (int x=0; x<WORLD_BUF; x++) for (int y=0; y<CHUNK_H; y++) for (int z=0; z<WORLD_BUF; z++) {
        unsigned char f = eng->faces[x][y][z]; if(!f) continue;
        float t = (float)eng->blocks[x][y][z], bx=(float)(ox+x), by=(float)y, bz=(float)(-(oz+z));
        float x0=bx-.5f,x1=bx+.5f,y0=by-.5f,y1=by+.5f,z0=bz-.5f,z1=bz+.5f;
        if(f&FACE_XP){ p_v(buf,&idx,x1,y0,z0,0,1, 1,0,0,t); p_v(buf,&idx,x1,y0,z1,1,1, 1,0,0,t); p_v(buf,&idx,x1,y1,z1,1,0, 1,0,0,t); p_v(buf,&idx,x1,y0,z0,0,1, 1,0,0,t); p_v(buf,&idx,x1,y1,z1,1,0, 1,0,0,t); p_v(buf,&idx,x1,y1,z0,0,0, 1,0,0,t); }
        if(f&FACE_XN){ p_v(buf,&idx,x0,y0,z1,0,1,-1,0,0,t); p_v(buf,&idx,x0,y0,z0,1,1,-1,0,0,t); p_v(buf,&idx,x0,y1,z0,1,0,-1,0,0,t); p_v(buf,&idx,x0,y0,z1,0,1,-1,0,0,t); p_v(buf,&idx,x0,y1,z0,1,0,-1,0,0,t); p_v(buf,&idx,x0,y1,z1,0,0,-1,0,0,t); }
        if(f&FACE_YP){ p_v(buf,&idx,x0,y1,z1,0,0, 0,1,0,t); p_v(buf,&idx,x1,y1,z1,1,0, 0,1,0,t); p_v(buf,&idx,x1,y1,z0,1,1, 0,1,0,t); p_v(buf,&idx,x0,y1,z1,0,0, 0,1,0,t); p_v(buf,&idx,x1,y1,z0,1,1, 0,1,0,t); p_v(buf,&idx,x0,y1,z0,0,1, 0,1,0,t); }
        if(f&FACE_YN){ p_v(buf,&idx,x0,y0,z0,0,0, 0,-1,0,t); p_v(buf,&idx,x1,y0,z0,1,0, 0,-1,0,t); p_v(buf,&idx,x1,y0,z1,1,1, 0,-1,0,t); p_v(buf,&idx,x0,y0,z0,0,0, 0,-1,0,t); p_v(buf,&idx,x1,y0,z1,1,1, 0,-1,0,t); p_v(buf,&idx,x0,y0,z1,0,1, 0,-1,0,t); }
        if(f&FACE_ZP){ p_v(buf,&idx,x1,y0,z0,0,1, 0,0,-1,t); p_v(buf,&idx,x0,y0,z0,1,1, 0,0,-1,t); p_v(buf,&idx,x0,y1,z0,1,0, 0,0,-1,t); p_v(buf,&idx,x1,y0,z0,0,1, 0,0,-1,t); p_v(buf,&idx,x0,y1,z0,1,0, 0,0,-1,t); p_v(buf,&idx,x1,y1,z0,0,0, 0,0,-1,t); }
        if(f&FACE_ZN){ p_v(buf,&idx,x0,y0,z1,0,1, 0,0,1,t); p_v(buf,&idx,x1,y0,z1,1,1, 0,0,1,t); p_v(buf,&idx,x1,y1,z1,1,0, 0,0,1,t); p_v(buf,&idx,x0,y0,z1,0,1, 0,0,1,t); p_v(buf,&idx,x1,y1,z1,1,0, 0,0,1,t); p_v(buf,&idx,x0,y1,z1,0,0, 0,0,1,t); }
    }
    if (!eng->vbo) glGenBuffers(1, &eng->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo); glBufferData(GL_ARRAY_BUFFER, fc*54*4, buf, GL_STATIC_DRAW);
    free(buf); eng->meshDirty = false;
}

static void render_anim(struct engine* eng) {
    if (!eng->animActive) return;
    float s = (eng->animIsBreak) ? (float)eng->animBreakTimer/ANIM_BREAK_FRAMES : 1.0f - (float)eng->animPlaceTimer/ANIM_PLACE_FRAMES;
    s *= s; if (eng->animBreakTimer > 0) eng->animBreakTimer--; if (eng->animPlaceTimer > 0) eng->animPlaceTimer--;
    if (eng->animBreakTimer<=0 && eng->animPlaceTimer<=0) eng->animActive=false;
    float bx=eng->animBlockX, by=eng->animBlockY, bz=eng->animBlockZ, hs=s*0.5f;
    float ab[324]; int ai=0;
    p_v(ab,&ai,bx+hs,by-hs,bz-hs,0,1,1,0,0,1); p_v(ab,&ai,bx+hs,by-hs,bz+hs,1,1,1,0,0,1); p_v(ab,&ai,bx+hs,by+hs,bz+hs,1,0,1,0,0,1);
    p_v(ab,&ai,bx+hs,by-hs,bz-hs,0,1,1,0,0,1); p_v(ab,&ai,bx+hs,by+hs,bz+hs,1,0,1,0,0,1); p_v(ab,&ai,bx+hs,by+hs,bz-hs,0,0,1,0,0,1);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,36,ab); glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES,0,6);
}

void render_world(struct engine* eng) {
    if (eng->meshDirty) rebuild_vbo(eng);
    glEnable(GL_DEPTH_TEST); glUseProgram(eng->program);
    glUniform3f(glGetUniformLocation(eng->program, "uCP"), eng->camPos[0], eng->camPos[1], eng->camPos[2]);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, eng->texGrassTop); glUniform1i(glGetUniformLocation(eng->program, "uT1"), 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, eng->texGrassSide); glUniform1i(glGetUniformLocation(eng->program, "uT2"), 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, eng->texGrassDown); glUniform1i(glGetUniformLocation(eng->program, "uT3"), 2);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, eng->texLeaves); glUniform1i(glGetUniformLocation(eng->program, "uT4"), 3);
    glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, eng->texTreeSide); glUniform1i(glGetUniformLocation(eng->program, "uT5"), 4);
    glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, eng->texTreeTop); glUniform1i(glGetUniformLocation(eng->program, "uT6"), 5);
    float p[16], v[16], mvp[16];
    mat4_perspective(p, GAME_FOV, (float)eng->width/eng->height, 0.1f, 100.0f);
    mat4_lookat(v, eng->camPos, eng->camRot[0], eng->camRot[1]); mat4_mul(mvp, p, v);
    glUniformMatrix4fv(glGetUniformLocation(eng->program, "m"), 1, GL_FALSE, mvp);
    if (eng->visibleFaceCount > 0) {
        glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,36,0); glEnableVertexAttribArray(0);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,36,(void*)12); glEnableVertexAttribArray(1);
        glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,36,(void*)20); glEnableVertexAttribArray(2);
        glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,36,(void*)32); glEnableVertexAttribArray(3);
        glDrawArrays(GL_TRIANGLES, 0, eng->visibleFaceCount * 6);
    }
    render_anim(eng);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void init_ui_shader(void) {
    const char* vS = "attribute vec2 aPos; attribute vec2 aUV; varying vec2 vUV; void main(){ gl_Position=vec4(aPos,0.0,1.0); vUV=aUV; }";
    const char* fS = "precision mediump float; varying vec2 vUV; uniform vec4 col; uniform sampler2D tex; uniform int useTex; void main(){ if(useTex==1) gl_FragColor=col*texture2D(tex,vUV); else gl_FragColor=col; }";
    uiProg = glCreateProgram();
    GLuint vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs, 1, &vS, NULL); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs, 1, &fS, NULL); glCompileShader(fs);
    glAttachShader(uiProg, vs); glAttachShader(uiProg, fs); 
    glBindAttribLocation(uiProg, 0, "aPos"); glBindAttribLocation(uiProg, 1, "aUV"); glLinkProgram(uiProg);
}

static void draw_r(float cx, float cy, float hw, float hh, int sw, int sh, float cr, float cg, float cb, float ca) {
    float nx=(cx/sw)*2-1, ny=1-(cy/sh)*2, rw=(hw/sw)*2, rh=(hh/sh)*2;
    float v[] = {nx-rw,ny-rh, nx+rw,ny-rh, nx+rw,ny+rh, nx-rw,ny-rh, nx+rw,ny+rh, nx-rw,ny+rh};
    glUseProgram(uiProg); glUniform4f(glGetUniformLocation(uiProg,"col"), cr,cg,cb,ca); glUniform1i(glGetUniformLocation(uiProg,"useTex"), 0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,v); glEnableVertexAttribArray(0); glDrawArrays(GL_TRIANGLES,0,6);
}

static void draw_t(float cx, float cy, float hw, float hh, int sw, int sh, GLuint tex) {
    float nx=(cx/sw)*2-1, ny=1-(cy/sh)*2, rw=(hw/sw)*2, rh=(hh/sh)*2;
    float v[] = {nx-rw,ny-rh,0,1, nx+rw,ny-rh,1,1, nx+rw,ny+rh,1,0, nx-rw,ny-rh,0,1, nx+rw,ny+rh,1,0, nx-rw,ny+rh,0,0};
    glUseProgram(uiProg); glUniform4f(glGetUniformLocation(uiProg,"col"), 1,1,1,1); glUniform1i(glGetUniformLocation(uiProg,"useTex"), 1);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tex);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,16,v); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,16,v+2); glEnableVertexAttribArray(1);
    glDrawArrays(GL_TRIANGLES,0,6); glDisableVertexAttribArray(1);
}

void draw_ui(struct engine* eng) {
    int sw=eng->width, sh=eng->height;
    glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    draw_r(sw/2, sh/2, 10, 1, sw, sh, 1,1,1,1); draw_r(sw/2, sh/2, 1, 10, sw, sh, 1,1,1,1);
    draw_r(JOY_X_OFFSET, sh-JOY_Y_OFFSET, STICK_RADIUS, STICK_RADIUS, sw, sh, 0,0,0,0.6f);
    draw_r(sw-JUMP_BTN_OFFSET, sh-JUMP_BTN_OFFSET, 30, 30, sw, sh, 0,0,0,0.6f);
    draw_r(sw-BREAK_BTN_X, BREAK_BTN_Y, 20, 20, sw, sh, 0,0,0,0.6f);
    draw_r(sw-PLACE_BTN_X, PLACE_BTN_Y, 20, 20, sw, sh, 0,0,0,0.6f);
    for(int i=0; i<INV_SLOTS; i++) {
        float sx = (sw-INV_SLOTS*50)/2 + i*50 + 25, sy = sh-INV_Y_OFFSET;
        draw_r(sx, sy, 22, 22, sw, sh, 0.1f, 0.1f, 0.1f, 0.6f);
        if(i == eng->selectedSlot) {
            draw_r(sx, sy-22, 22, 2, sw, sh, 1,1,1,1); draw_r(sx, sy+22, 22, 2, sw, sh, 1,1,1,1);
            draw_r(sx-22, sy, 2, 22, sw, sh, 1,1,1,1); draw_r(sx+22, sy, 2, 22, sw, sh, 1,1,1,1);
        }
        GLuint icon = 0;
        if(eng->invSlots[i]==BLOCK_GRASS) icon=eng->texGrassSide;
        else if(eng->invSlots[i]==BLOCK_WOOD) icon=eng->texTreeSide;
        else if(eng->invSlots[i]==BLOCK_LEAVES) icon=eng->texLeaves;
        if(icon) draw_t(sx, sy, 18, 18, sw, sh, icon);
    }
    glDisable(GL_BLEND);
}

void draw_menu(struct engine* eng) {
    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    draw_r(eng->width/2, eng->height/2, 100, 40, eng->width, eng->height, 1,1,1,1);
}
#endif
