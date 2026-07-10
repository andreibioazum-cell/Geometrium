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
    eng->texLeaves = load_texture(eng->app, "foliage.png");
    eng->texTreeSide = load_texture(eng->app, "tree_side.png");
    eng->texTreeTop = load_texture(eng->app, "tree_top_down.png");
    if (!eng->texLeaves) eng->texLeaves = make_color_tex(40, 120, 40);
    if (!eng->texTreeSide) eng->texTreeSide = make_color_tex(90, 70, 40);
}

static void push_quad_n(float* buf, int* idx, float x0,float y0,float z0,float u0,float v0, float x1,float y1,float z1,float u1,float v1, float x2,float y2,float z2,float u2,float v2, float x3,float y3,float z3,float u3,float v3, float nx,float ny,float nz, float type) {
    float vd[6][9] = { {x0,y0,z0,u0,v0,nx,ny,nz,type},{x1,y1,z1,u1,v1,nx,ny,nz,type}, {x2,y2,z2,u2,v2,nx,ny,nz,type},{x0,y0,z0,u0,v0,nx,ny,nz,type}, {x2,y2,z2,u2,v2,nx,ny,nz,type},{x3,y3,z3,u3,v3,nx,ny,nz,type}};
    memcpy(&buf[*idx], vd, sizeof(vd)); *idx += 54;
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
    if (fc == 0) { eng->meshDirty = false; return; }
    int cnt = fc * 54;
    float* buf = (float*)malloc((size_t)cnt * sizeof(float));
    int idx = 0, ox = eng->loadCenterX - LOAD_RADIUS, oz = eng->loadCenterZ - LOAD_RADIUS;
    for (int x = 0; x < WORLD_BUF; x++)
        for (int y = 0; y < CHUNK_H; y++)
            for (int z = 0; z < WORLD_BUF; z++) {
                unsigned char f = eng->faces[x][y][z]; if(!f) continue;
                float type = (float)eng->blocks[x][y][z];
                float bx=(float)(ox+x),by=(float)y,bz=(float)(-(oz+z));
                float x0=bx-.5f,x1=bx+.5f,y0=by-.5f,y1=by+.5f,z0=bz-.5f,z1=bz+.5f;
                if(f&FACE_XP)push_quad_n(buf,&idx,x1,y0,z0,0,1,x1,y0,z1,1,1,x1,y1,z1,1,0,x1,y1,z0,0,0,1,0,0,type);
                if(f&FACE_XN)push_quad_n(buf,&idx,x0,y0,z1,0,1,x0,y0,z0,1,1,x0,y1,z0,1,0,x0,y1,z1,0,0,-1,0,0,type);
                if(f&FACE_YP)push_quad_n(buf,&idx,x0,y1,z1,0,0,x1,y1,z1,1,0,x1,y1,z0,1,1,x0,y1,z0,0,1,0,1,0,type);
                if(f&FACE_YN)push_quad_n(buf,&idx,x0,y0,z0,0,0,x1,y0,z0,1,0,x1,y0,z1,1,1,x0,y0,z1,0,1,0,-1,0,type);
                if(f&FACE_ZP)push_quad_n(buf,&idx,x1,y0,z0,0,1,x0,y0,z0,1,1,x0,y1,z0,1,0,x1,y1,z0,0,0,0,0,-1,type);
                if(f&FACE_ZN)push_quad_n(buf,&idx,x0,y0,z1,0,1,x1,y0,z1,1,1,x1,y1,z1,1,0,x0,y1,z1,0,0,0,0,1,type);
            }
    if (!eng->vbo) glGenBuffers(1, &eng->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo); glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(cnt*sizeof(float)), buf, GL_STATIC_DRAW);
    free(buf); eng->meshDirty = false;
}

// ПЛАВНАЯ АНИМАЦИЯ МАСШТАБА
static void render_anim_block(struct engine* eng) {
    if (!eng->animActive) return;
    float scale = 1.0f;
    if (eng->animIsBreak) {
        float t = (float)eng->animBreakTimer / ANIM_BREAK_FRAMES;
        scale = t * t; // Уменьшение
        eng->animBreakTimer--;
    } else {
        float t = (float)eng->animPlaceTimer / ANIM_PLACE_FRAMES;
        scale = 1.0f - (t * t); // Увеличение
        eng->animPlaceTimer--;
    }
    if (eng->animBreakTimer <= 0 && eng->animPlaceTimer <= 0) eng->animActive = false;
    
    float bx = eng->animBlockX, by = eng->animBlockY, bz = eng->animBlockZ, hs = scale * 0.5f;
    float abuf[324]; int aidx = 0; float type = 1.0f; // Для анимации используем траву
    push_quad_n(abuf,&aidx,bx+hs,by-hs,bz-hs,0,1,bx+hs,by-hs,bz+hs,1,1,bx+hs,by+hs,bz+hs,1,0,bx+hs,by+hs,bz-hs,0,0,1,0,0,type);
    push_quad_n(abuf,&aidx,bx-hs,by-hs,bz+hs,0,1,bx-hs,by-hs,bz-hs,1,1,bx-hs,by+hs,bz-hs,1,0,bx-hs,by+hs,bz+hs,0,0,-1,0,0,type);
    push_quad_n(abuf,&aidx,bx-hs,by+hs,bz+hs,0,0,bx+hs,by+hs,bz+hs,1,0,bx+hs,by+hs,bz-hs,1,1,bx-hs,by+hs,bz-hs,0,1,0,1,0,type);
    push_quad_n(abuf,&aidx,bx-hs,by-hs,bz-hs,0,0,bx+hs,by-hs,bz-hs,1,0,bx+hs,by-hs,bz+hs,1,1,bx-hs,by-hs,bz+hs,0,1,0,-1,0,type);
    push_quad_n(abuf,&aidx,bx+hs,by-hs,bz-hs,0,1,bx-hs,by-hs,bz-hs,1,1,bx-hs,by+hs,bz-hs,1,0,bx+hs,by+hs,bz-hs,0,0,0,0,-1,type);
    push_quad_n(abuf,&aidx,bx-hs,by-hs,bz+hs,0,1,bx+hs,by-hs,bz+hs,1,1,bx+hs,by+hs,bz+hs,1,0,bx-hs,by+hs,bz+hs,0,0,0,0,1,type);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,36,abuf); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,36,abuf+3); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,36,abuf+5); glEnableVertexAttribArray(2);
    glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,36,abuf+8); glEnableVertexAttribArray(3);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glDisableVertexAttribArray(1); glDisableVertexAttribArray(2); glDisableVertexAttribArray(3);
}

void render_world(struct engine* eng) {
    if (eng->meshDirty) rebuild_vbo(eng);
    glEnable(GL_DEPTH_TEST); glUseProgram(eng->program);
    glUniform3f(glGetUniformLocation(eng->program, "camPos"), eng->camPos[0], eng->camPos[1], eng->camPos[2]);
    
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, eng->texGrassTop); glUniform1i(glGetUniformLocation(eng->program, "texTop"), 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, eng->texGrassSide); glUniform1i(glGetUniformLocation(eng->program, "texSide"), 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, eng->texGrassDown); glUniform1i(glGetUniformLocation(eng->program, "texDown"), 2);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, eng->texLeaves); glUniform1i(glGetUniformLocation(eng->program, "texLeaves"), 3);
    glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, eng->texTreeSide); glUniform1i(glGetUniformLocation(eng->program, "texTreeSide"), 4);
    glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, eng->texTreeTop); glUniform1i(glGetUniformLocation(eng->program, "texTreeTop"), 5);
    
    float proj[16], view[16], mvp[16];
    mat4_perspective(proj, GAME_FOV, (float)eng->width/(float)eng->height, 0.1f, 200.0f);
    mat4_lookat(view, eng->camPos, eng->camRot[0], eng->camRot[1]);
    mat4_mul(mvp, proj, view);
    glUniformMatrix4fv(glGetUniformLocation(eng->program, "m"), 1, GL_FALSE, mvp);
    
    if (eng->visibleFaceCount > 0) {
        glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,36,(void*)0); glEnableVertexAttribArray(0);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,36,(void*)12); glEnableVertexAttribArray(1);
        glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,36,(void*)20); glEnableVertexAttribArray(2);
        glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,36,(void*)32); glEnableVertexAttribArray(3);
        glDrawArrays(GL_TRIANGLES, 0, eng->visibleFaceCount * 6);
    }
    render_anim_block(eng);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glDisableVertexAttribArray(1); glDisableVertexAttribArray(2); glDisableVertexAttribArray(3);
}

void init_ui_shader(void) {
    const char* vS = "attribute vec2 aPos; attribute vec2 aUV; varying vec2 vUV; void main(){ gl_Position=vec4(aPos,0.0,1.0); vUV=aUV; }";
    const char* fS = "precision mediump float; varying vec2 vUV; uniform vec4 col; uniform sampler2D tex; uniform int useTex; void main(){ if(useTex==1) gl_FragColor=col*texture2D(tex,vUV); else gl_FragColor=col; }";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs, 1, &vS, NULL); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs, 1, &fS, NULL); glCompileShader(fs);
    uiProg = glCreateProgram(); glAttachShader(uiProg, vs); glAttachShader(uiProg, fs); 
    glBindAttribLocation(uiProg, 0, "aPos"); glBindAttribLocation(uiProg, 1, "aUV"); glLinkProgram(uiProg);
}

static void draw_rect_no_tex(float cx, float cy, float hw, float hh, int sw, int sh, float cr, float cg, float cb, float ca) {
    float nx = (cx/sw)*2.0f-1.0f, ny = 1.0f-(cy/sh)*2.0f, rw = (hw/sw)*2.0f, rh = (hh/sh)*2.0f;
    float v[] = {nx-rw, ny-rh, nx+rw, ny-rh, nx+rw, ny+rh, nx-rw, ny-rh, nx+rw, ny+rh, nx-rw, ny+rh};
    glUseProgram(uiProg); glUniform4f(glGetUniformLocation(uiProg,"col"), cr,cg,cb,ca); glUniform1i(glGetUniformLocation(uiProg, "useTex"), 0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,v); glEnableVertexAttribArray(0); glDrawArrays(GL_TRIANGLES,0,6);
}

static void draw_rect_tex(float cx, float cy, float hw, float hh, int sw, int sh, GLuint tex) {
    float nx = (cx/sw)*2.0f-1.0f, ny = 1.0f-(cy/sh)*2.0f, rw = (hw/sw)*2.0f, rh = (hh/sh)*2.0f;
    float v[] = {nx-rw,ny-rh,0,1, nx+rw,ny-rh,1,1, nx+rw,ny+rh,1,0, nx-rw,ny-rh,0,1, nx+rw,ny+rh,1,0, nx-rw,ny+rh,0,0};
    glUseProgram(uiProg); glUniform4f(glGetUniformLocation(uiProg,"col"), 1,1,1,1); glUniform1i(glGetUniformLocation(uiProg, "useTex"), 1);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tex);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,16,v); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,16,v+2); glEnableVertexAttribArray(1);
    glDrawArrays(GL_TRIANGLES,0,6); glDisableVertexAttribArray(1);
}

static void draw_ring(float cx, float cy, float r, float thick, int w, int h, float cr, float cg, float cb, float ca) {
    float ndcX=(cx/w)*2-1, ndcY=1-(cy/h)*2, rxo=(r/w)*2, ryo=(r/h)*2, rxi=((r-thick)/w)*2, ryi=((r-thick)/h)*2;
    float v[132]; for(int i=0;i<=32;i++){ float a=i/32.0f*2*PI, c=cosf(a), s=sinf(a); v[i*4]=ndcX+c*rxo; v[i*4+1]=ndcY+s*ryo; v[i*4+2]=ndcX+c*rxi; v[i*4+3]=ndcY+s*ryi; }
    glUseProgram(uiProg); glUniform4f(glGetUniformLocation(uiProg,"col"), cr,cg,cb,ca); glUniform1i(glGetUniformLocation(uiProg, "useTex"), 0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,v); glEnableVertexAttribArray(0); glDrawArrays(GL_TRIANGLE_STRIP,0,66);
}

static void draw_circle(float cx, float cy, float r, int w, int h, float cr, float cg, float cb, float ca) {
    float ndcX=(cx/w)*2-1, ndcY=1-(cy/h)*2, rx=(r/w)*2, ry=(r/h)*2;
    float v[52]; v[0]=ndcX; v[1]=ndcY; for(int i=0;i<=24;i++){ float a=i/24.0f*2*PI; v[(i+1)*2]=ndcX+cosf(a)*rx; v[(i+1)*2+1]=ndcY+sinf(a)*ry; }
    glUseProgram(uiProg); glUniform4f(glGetUniformLocation(uiProg,"col"), cr,cg,cb,ca); glUniform1i(glGetUniformLocation(uiProg, "useTex"), 0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,v); glEnableVertexAttribArray(0); glDrawArrays(GL_TRIANGLE_FAN,0,26);
}

static void draw_border(float cx, float cy, float hw, float hh, float t, int sw, int sh, float cr, float cg, float cb, float ca) {
    draw_rect_no_tex(cx, cy-hh+t*0.5f, hw, t*0.5f, sw, sh, cr,cg,cb,ca); draw_rect_no_tex(cx, cy+hh-t*0.5f, hw, t*0.5f, sw, sh, cr,cg,cb,ca);
    draw_rect_no_tex(cx-hw+t*0.5f, cy, t*0.5f, hh, sw, sh, cr,cg,cb,ca); draw_rect_no_tex(cx+hw-t*0.5f, cy, t*0.5f, hh, sw, sh, cr,cg,cb,ca);
}

void draw_ui(struct engine* eng) {
    int sw=eng->width, sh=eng->height;
    glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    draw_rect_no_tex(sw/2.0f, sh/2.0f, 12, 1.5f, sw, sh, 1,1,1,0.8f); draw_rect_no_tex(sw/2.0f, sh/2.0f, 1.5f, 12, sw, sh, 1,1,1,0.8f);
    float jx = JOY_X_OFFSET, jy = sh - JOY_Y_OFFSET;
    draw_ring(jx, jy, JOY_RADIUS, 4, sw, sh, 0,0,0,0.5f); draw_circle(jx + eng->moveDirX*JOY_RADIUS*0.6f, jy + eng->moveDirZ*JOY_RADIUS*0.6f, STICK_RADIUS, sw, sh, 0,0,0,0.8f);
    float bx = sw - JUMP_BTN_OFFSET, by = sh - JUMP_BTN_OFFSET;
    draw_ring(bx, by, JUMP_BTN_SIZE, 4, sw, sh, 0,0,0,0.5f);
    float nx = (bx/sw)*2.0f-1.0f, ny = 1.0f-(by/sh)*2.0f, asx = (20.0f/sw)*2.0f, asy = (20.0f/sh)*2.0f;
    float arrow[] = { nx, ny+asy, nx-asx, ny-asy*0.5f, nx+asx, ny-asy*0.5f };
    glUseProgram(uiProg); glUniform4f(glGetUniformLocation(uiProg,"col"), 0,0,0,0.8f); glUniform1i(glGetUniformLocation(uiProg, "useTex"), 0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,arrow); glEnableVertexAttribArray(0); glDrawArrays(GL_TRIANGLES, 0, 3);
    draw_ring(sw-BREAK_BTN_X, BREAK_BTN_Y, ACTION_BTN_SIZE, 3, sw, sh, 0,0,0,0.5f); draw_rect_no_tex(sw-BREAK_BTN_X, BREAK_BTN_Y, 8, 2, sw, sh, 0,0,0,1); 
    draw_ring(sw-PLACE_BTN_X, PLACE_BTN_Y, ACTION_BTN_SIZE, 3, sw, sh, 0,0,0,0.5f); draw_rect_no_tex(sw-PLACE_BTN_X, PLACE_BTN_Y, 8, 2, sw, sh, 0,0,0,1); draw_rect_no_tex(sw-PLACE_BTN_X, PLACE_BTN_Y, 2, 8, sw, sh, 0,0,0,1);
    float invW = INV_SLOTS*(INV_SLOT_SIZE+INV_PADDING), invStartX = (sw - invW) / 2.0f;
    for(int i=0; i<INV_SLOTS; i++) {
        float sx = invStartX + i*(INV_SLOT_SIZE+INV_PADDING) + INV_SLOT_SIZE/2.0f, sy = sh - INV_Y_OFFSET;
        draw_rect_no_tex(sx, sy, INV_SLOT_SIZE/2.0f, INV_SLOT_SIZE/2.0f, sw, sh, 0.1f, 0.1f, 0.1f, 0.6f);
        if(i == eng->selectedSlot) draw_border(sx, sy, INV_SLOT_SIZE/2.0f, INV_SLOT_SIZE/2.0f, 3, sw, sh, 1,1,1,0.9f);
        GLuint icon = 0;
        if(eng->invSlots[i] == BLOCK_GRASS) icon = eng->texGrassSide;
        else if(eng->invSlots[i] == BLOCK_WOOD) icon = eng->texTreeSide;
        else if(eng->invSlots[i] == BLOCK_LEAVES) icon = eng->texLeaves;
        if(icon) draw_rect_tex(sx, sy, INV_SLOT_SIZE*0.35f, INV_SLOT_SIZE*0.35f, sw, sh, icon);
    }
    glDisable(GL_BLEND);
}
#endif
