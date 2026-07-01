#ifndef RENDER_H
#define RENDER_H
#include "engine.h"
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static GLuint load_tex(struct android_app* app, const char* name) {
    AAsset* a = AAssetManager_open(app->activity->assetManager, name, AASSET_MODE_BUFFER);
    if(!a) return 0;
    int l = AAsset_getLength(a); unsigned char* b = malloc(l);
    AAsset_read(a, b, l); AAsset_close(a);
    int w, h, n; unsigned char* img = stbi_load_from_memory(b, l, &w, &h, &n, 4);
    free(b); if(!img) return 0;
    GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, 10241, 9728); glTexParameteri(GL_TEXTURE_2D, 10240, 9728);
    glTexImage2D(GL_TEXTURE_2D, 0, 6408, w, h, 0, 6408, 5121, img);
    stbi_image_free(img); return t;
}

static void push_q(float* b, int* p, float* v, float nx, float ny, float nz) {
    float u[]={0,1, 1,1, 1,0, 0,1, 1,0, 0,0};
    int id[]={0,1,2, 0,2,3};
    for(int i=0; i<6; i++) {
        int vi = id[i];
        b[(*p)++] = v[vi*3]; b[(*p)++] = v[vi*3+1]; b[(*p)++] = v[vi*3+2];
        b[(*p)++] = u[i*2]; b[(*p)++] = u[i*2+1];
        b[(*p)++] = nx; b[(*p)++] = ny; b[(*p)++] = nz;
    }
}

static void rebuild_mesh(struct engine* eng) {
    int fc = 0;
    for(int x=0; x<WORLD_BUF; x++) for(int y=0; y<CHUNK_H; y++) for(int z=0; z<WORLD_BUF; z++) {
        if(!eng->blocks[x][y][z]) continue;
        unsigned char f=0;
        #define B(dx,dy,dz) ( (x+dx>=0 && x+dx<WORLD_BUF && z+dz>=0 && z+dz<WORLD_BUF) ? eng->blocks[x+dx][y+dy][z+dz] : 0 )
        if(!B(1,0,0)) f|=FACE_XP; if(!B(-1,0,0)) f|=FACE_XN;
        if(y==CHUNK_H-1 || !eng->blocks[x][y+1][z]) f|=FACE_YP;
        if(y==0 || !eng->blocks[x][y-1][z]) f|=FACE_YN;
        if(!B(0,0,1)) f|=FACE_ZP; if(!B(0,0,-1)) f|=FACE_ZN;
        eng->faces[x][y][z]=f; for(int i=0;i<6;i++) if(f&(1<<i)) fc++;
    }
    eng->visibleFaceCount = fc; if(!fc) return;
    float* buf = malloc(fc*48*4); int p=0;
    int ox=eng->loadCenterX-LOAD_RADIUS, oz=eng->loadCenterZ-LOAD_RADIUS;
    for(int x=0; x<WORLD_BUF; x++) for(int y=0; y<CHUNK_H; y++) for(int z=0; z<WORLD_BUF; z++) {
        unsigned char f=eng->faces[x][y][z]; if(!f) continue;
        float x0=ox+x-.5f, x1=ox+x+.5f, y0=y-.5f, y1=y+.5f, z0=-(oz+z)-.5f, z1=-(oz+z)+.5f;
        if(f&FACE_XP){ float v[]={x1,y0,z0, x1,y0,z1, x1,y1,z1, x1,y1,z0}; push_q(buf,&p,v,1,0,0); }
        if(f&FACE_XN){ float v[]={x0,y0,z1, x0,y0,z0, x0,y1,z0, x0,y1,z1}; push_q(buf,&p,v,-1,0,0); }
        if(f&FACE_YP){ float v[]={x0,y1,z1, x1,y1,z1, x1,y1,z0, x0,y1,z0}; push_q(buf,&p,v,0,1,0); }
        if(f&FACE_YN){ float v[]={x0,y0,z0, x1,y0,z0, x1,y0,z1, x0,y0,z1}; push_q(buf,&p,v,0,-1,0); }
        if(f&FACE_ZP){ float v[]={x1,y0,z1, x0,y0,z1, x0,y1,z1, x1,y1,z1}; push_q(buf,&p,v,0,0,1); }
        if(f&FACE_ZN){ float v[]={x0,y0,z0, x1,y0,z0, x1,y1,z0, x0,y1,z0}; push_q(buf,&p,v,0,0,-1); }
    }
    if(!eng->vbo) glGenBuffers(1, &eng->vbo);
    glBindBuffer(34962, eng->vbo); glBufferData(34962, fc*48*4, buf, 35044); free(buf);
    eng->meshDirty=false;
}

// Функции UI (кольца, цифры и т.д.)
static void draw_rect(struct engine* eng, float x, float y, float w, float h, float r, float g, float b, float a) {
    float nx=(x/eng->width)*2-1, ny=1-(y/eng->height)*2, rw=(w/eng->width)*2, rh=(h/eng->height)*2;
    float v[]={nx-rw,ny-rh, nx+rw,ny-rh, nx+rw,ny+rh, nx-rw,ny-rh, nx+rw,ny+rh, nx-rw,ny+rh};
    glUseProgram(eng->uiProg); glUniform4f(glGetUniformLocation(eng->uiProg,"col"), r,g,b,a);
    glVertexAttribPointer(0,2,5126,0,0,v); glEnableVertexAttribArray(0); glDrawArrays(4,0,6);
}

static void draw_digit(struct engine* eng, float cx, float cy, float sz, int d, float r, float g, float b) {
    float w=sz*0.4f, h=sz*0.5f, t=sz*0.1f;
    bool s[10][7]={{1,0,1,1,1,1,1},{0,0,0,0,1,0,1},{1,1,1,0,1,1,0},{1,1,1,0,1,0,1},{0,1,0,1,1,0,1},{1,1,1,1,0,0,1},{1,1,1,1,0,1,1},{1,0,0,0,1,0,1},{1,1,1,1,1,1,1},{1,1,1,1,1,0,1}};
    if(s[d][0]) draw_rect(eng,cx,cy-h,w,t,r,g,b,1); if(s[d][1]) draw_rect(eng,cx,cy,w,t,r,g,b,1); if(s[d][2]) draw_rect(eng,cx,cy+h,w,t,r,g,b,1);
    if(s[d][3]) draw_rect(eng,cx-w,cy-h*0.5f,t,h*0.5f,r,g,b,1); if(s[d][4]) draw_rect(eng,cx+w,cy-h*0.5f,t,h*0.5f,r,g,b,1);
    if(s[d][5]) draw_rect(eng,cx-w,cy+h*0.5f,t,h*0.5f,r,g,b,1); if(s[d][6]) draw_rect(eng,cx+w,cy+h*0.5f,t,h*0.5f,r,g,b,1);
}

static void draw_ui(struct engine* eng) {
    glDisable(2929); glEnable(3042); glBlendFunc(770, 771);
    if(eng->gameState == STATE_MENU) {
        draw_rect(eng, eng->width/2, eng->height/2, eng->width/2, eng->height/2, 0.2, 0.6, 0.3, 1);
        for(int i=0; i<eng->seedCursor; i++) draw_digit(eng, eng->width/2-80+i*30, eng->height/2, 20, eng->seedDigits[i], 1,1,1);
    } else {
        draw_rect(eng, eng->width/2, eng->height/2, 10, 1, 1,1,1,1); draw_rect(eng, eng->width/2, eng->height/2, 1, 10, 1,1,1,1);
        draw_rect(eng, JOY_X_OFFSET, eng->height-JOY_Y_OFFSET, JOY_RADIUS, JOY_RADIUS, 0,0,0,0.2);
        draw_rect(eng, eng->width-BREAK_BTN_X, BREAK_BTN_Y, ACTION_BTN_SIZE, ACTION_BTN_SIZE, 1,0,0,0.4);
        draw_rect(eng, eng->width-PLACE_BTN_X, PLACE_BTN_Y, ACTION_BTN_SIZE, ACTION_BTN_SIZE, 0,1,0,0.4);
    }
    glDisable(3042);
}

static void draw_world(struct engine* eng) {
    if(eng->meshDirty) rebuild_mesh(eng);
    glEnable(2929); glUseProgram(eng->program);
    float p[16], v[16], m[16]; mat4_perspective(p, GAME_FOV, (float)eng->width/eng->height, 0.1, 100);
    mat4_lookat(v, eng->camPos, eng->camRot[0], eng->camRot[1]); mat4_mul(m, p, v);
    glUniformMatrix4fv(glGetUniformLocation(eng->program,"m"), 1, 0, m);
    glActiveTexture(33984); glBindTexture(3553, eng->texGrassTop);
    glActiveTexture(33985); glBindTexture(3553, eng->texGrassSide);
    glActiveTexture(33986); glBindTexture(3553, eng->texGrassDown);
    glBindBuffer(34962, eng->vbo);
    glVertexAttribPointer(0,3,5126,0,32,0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,5126,0,32,(void*)12); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,3,5126,0,32,(void*)20); glEnableVertexAttribArray(2);
    glDrawArrays(4, 0, eng->visibleFaceCount*6);
}
#endif
