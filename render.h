#ifndef RENDER_H
#define RENDER_H
#include "engine.h"
#include "math_utils.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
static GLuint uiP=0;
static GLuint load_t(struct android_app* a, const char* n) {
    AAsset* s=AAssetManager_open(a->activity->assetManager, n, AASSET_MODE_BUFFER); if(!s) return 0;
    int w,h,c; unsigned char* d=stbi_load_from_memory((void*)AAsset_getBuffer(s), AAsset_getLength(s), &w,&h,&c,4);
    GLuint t; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
    glTexParameteri(GL_TEXTURE_2D,10240,9728); glTexParameteri(GL_TEXTURE_2D,10241,9728);
    glTexImage2D(GL_TEXTURE_2D,0,6408,w,h,0,6408,5121,d); free(d); AAsset_close(s); return t;
}
void init_textures(struct engine* e) {
    e->texGrassTop=load_t(e->app,"grass_top.png"); e->texGrassSide=load_t(e->app,"grass_side.png"); e->texGrassDown=load_t(e->app,"grass_down.png");
    e->texLeaves=load_t(e->app,"foliage.png"); e->texTreeSide=load_t(e->app,"tree_side.png"); e->texTreeTop=load_t(e->app,"tree_top_down.png");
}
static void pv(float* b, int* i, float x, float y, float z, float u, float v, float nx, float ny, float nz, float t) {
    float d[]={x,y,z,u,v,nx,ny,nz,t}; memcpy(&b[*i],d,36); *i+=9;
}
static void rebuild_vbo(struct engine* e) {
    update_faces(e); int fc=0; for(int x=0;x<29;x++) for(int y=0;y<32;y++) for(int z=0;z<29;z++) {
        unsigned char f=e->faces[x][y][z]; if(f&1)fc++; if(f&2)fc++; if(f&4)fc++; if(f&8)fc++; if(f&16)fc++; if(f&32)fc++;
    }
    e->visibleFaceCount=fc; if(!fc) return; float* b=(float*)malloc(fc*54*4); int idx=0;
    for(int x=0;x<29;x++) for(int y=0;y<32;y++) for(int z=0;z<29;z++) {
        unsigned char f=e->faces[x][y][z]; if(!f)continue; float t=(float)e->blocks[x][y][z], bx=e->loadCenterX-14+x, by=y, bz=-(e->loadCenterZ-14+z);
        float x0=bx-.5f,x1=bx+.5f,y0=by-.5f,y1=by+.5f,z0=bz-.5f,z1=bz+.5f;
        if(f&1){pv(b,&idx,x1,y0,z0,0,1,1,0,0,t);pv(b,&idx,x1,y0,z1,1,1,1,0,0,t);pv(b,&idx,x1,y1,z1,1,0,1,0,0,t);pv(b,&idx,x1,y0,z0,0,1,1,0,0,t);pv(b,&idx,x1,y1,z1,1,0,1,0,0,t);pv(b,&idx,x1,y1,z0,0,0,1,0,0,t);}
        if(f&2){pv(b,&idx,x0,y0,z1,0,1,-1,0,0,t);pv(b,&idx,x0,y0,z0,1,1,-1,0,0,t);pv(b,&idx,x0,y1,z0,1,0,-1,0,0,t);pv(b,&idx,x0,y0,z1,0,1,-1,0,0,t);pv(b,&idx,x0,y1,z0,1,0,-1,0,0,t);pv(b,&idx,x0,y1,z1,0,0,-1,0,0,t);}
        if(f&4){pv(b,&idx,x0,y1,z1,0,0,0,1,0,t);pv(b,&idx,x1,y1,z1,1,0,0,1,0,t);pv(b,&idx,x1,y1,z0,1,1,0,1,0,t);pv(b,&idx,x0,y1,z1,0,0,0,1,0,t);pv(b,&idx,x1,y1,z0,1,1,0,1,0,t);pv(b,&idx,x0,y1,z0,0,1,0,1,0,t);}
        if(f&8){pv(b,&idx,x0,y0,z0,0,0,0,-1,0,t);pv(b,&idx,x1,y0,z0,1,0,0,-1,0,t);pv(b,&idx,x1,y0,z1,1,1,0,-1,0,t);pv(b,&idx,x0,y0,z0,0,0,0,-1,0,t);pv(b,&idx,x1,y0,z1,1,1,0,-1,0,t);pv(b,&idx,x0,y0,z1,0,1,0,-1,0,t);}
        if(f&16){pv(b,&idx,x1,y0,z0,0,1,0,0,-1,t);pv(b,&idx,x0,y0,z0,1,1,0,0,-1,t);pv(b,&idx,x0,y1,z0,1,0,0,0,-1,t);pv(b,&idx,x1,y0,z0,0,1,0,0,-1,t);pv(b,&idx,x0,y1,z0,1,0,0,0,-1,t);pv(b,&idx,x1,y1,z0,0,0,0,0,-1,t);}
        if(f&32){pv(b,&idx,x0,y0,z1,0,1,0,0,1,t);pv(b,&idx,x1,y0,z1,1,1,0,0,1,t);pv(b,&idx,x1,y1,z1,1,0,0,0,1,t);pv(b,&idx,x0,y0,z1,0,1,0,0,1,t);pv(b,&idx,x1,y1,z1,1,0,0,0,1,t);pv(b,&idx,x0,y1,z1,0,0,0,0,1,t);} // исправлено: добавлен 0 (nz) между ny и nz
    }
    if(!e->vbo)glGenBuffers(1,&e->vbo); glBindBuffer(34962,e->vbo); glBufferData(34962,fc*54*4,b,35044); free(b); e->meshDirty=false;
}
static void render_anim(struct engine* e) {
    if(!e->animActive)return; float s=(e->animIsBreak)?(float)e->animBreakTimer/15:(1.0f-(float)e->animPlaceTimer/12);
    s*=s; if(e->animBreakTimer>0)e->animBreakTimer--; if(e->animPlaceTimer>0)e->animPlaceTimer--; if(e->animBreakTimer<=0&&e->animPlaceTimer<=0)e->animActive=false;
    float bx=e->animBlockX,by=e->animBlockY,bz=e->animBlockZ,hs=s*.5f,t=(float)e->animBlockType,ab[324]; int ai=0;
    pv(ab,&ai,bx+hs,by-hs,bz-hs,0,1,1,0,0,t);pv(ab,&ai,bx+hs,by-hs,bz+hs,1,1,1,0,0,t);pv(ab,&ai,bx+hs,by+hs,bz+hs,1,0,1,0,0,t);pv(ab,&ai,bx+hs,by-hs,bz-hs,0,1,1,0,0,t);pv(ab,&ai,bx+hs,by+hs,bz+hs,1,0,1,0,0,t);pv(ab,&ai,bx+hs,by+hs,bz-hs,0,0,1,0,0,t);
    glBindBuffer(34962,0); glVertexAttribPointer(0,3,5126,0,36,ab); glEnableVertexAttribArray(0); glVertexAttribPointer(3,1,5126,0,36,(void*)32); glEnableVertexAttribArray(3); glDrawArrays(4,0,6);
}
void render_world(struct engine* e) {
    if(e->meshDirty)rebuild_vbo(e); glEnable(2929); glEnable(2884); glUseProgram(e->program);
    glUniform3f(glGetUniformLocation(e->program,"uCP"),e->camPos[0],e->camPos[1],e->camPos[2]);
    float p[16],v[16],mvp[16]; mat4_perspective(p,1.49f,(float)e->width/e->height,0.1f,100.f); mat4_lookat(v,e->camPos,e->camRot[0],e->camRot[1]); mat4_mul(mvp,p,v);
    glUniformMatrix4fv(glGetUniformLocation(e->program,"m"),1,0,mvp);
    GLuint texs[]={e->texGrassTop,e->texGrassSide,e->texGrassDown,e->texLeaves,e->texTreeSide,e->texTreeTop};
    for(int i=0;i<6;i++){glActiveTexture(33984+i);glBindTexture(3553,texs[i]);char n[4]="uT1";n[2]='1'+i;glUniform1i(glGetUniformLocation(e->program,n),i);}
    if(e->visibleFaceCount>0){glBindBuffer(34962,e->vbo);glVertexAttribPointer(0,3,5126,0,36,0);glEnableVertexAttribArray(0);glVertexAttribPointer(1,2,5126,0,36,(void*)12);glEnableVertexAttribArray(1);glVertexAttribPointer(2,3,5126,0,36,(void*)20);glEnableVertexAttribArray(2);glVertexAttribPointer(3,1,5126,0,36,(void*)32);glEnableVertexAttribArray(3);glDrawArrays(4,0,e->visibleFaceCount*6);}
    render_anim(e);
}
void init_ui_shader(void) {
    const char *vs="attribute vec2 a;attribute vec2 b;varying vec2 v;void main(){gl_Position=vec4(a,0,1);v=b;}", *fs="precision mediump float;varying vec2 v;uniform vec4 c;uniform sampler2D t;uniform int u;void main(){gl_FragColor=(u==1)?c*texture2D(t,v):c;}";
    uiP=glCreateProgram(); GLuint s=glCreateShader(35633); glShaderSource(s,1,&vs,0); glCompileShader(s); glAttachShader(uiP,s); s=glCreateShader(35632); glShaderSource(s,1,&fs,0); glCompileShader(s); glAttachShader(uiP,s); glBindAttribLocation(uiP,0,"a"); glBindAttribLocation(uiP,1,"b"); glLinkProgram(uiP);
}
static void dr(float x,float y,float w,float h,int sw,int sh,float r,float g,float b,float a){
    float nx=x/sw*2-1,ny=1-y/sh*2,rw=w/sw,rh=h/sh,v[]={nx-rw,ny-rh,nx+rw,ny-rh,nx+rw,ny+rh,nx-rw,ny-rh,nx+rw,ny+rh,nx-rw,ny+rh};
    glUseProgram(uiP); glUniform4f(glGetUniformLocation(uiP,"c"),r,g,b,a); glUniform1i(glGetUniformLocation(uiP,"u"),0); glVertexAttribPointer(0,2,5126,0,0,v); glEnableVertexAttribArray(0); glDrawArrays(4,0,6);
}
void draw_ui(struct engine* e){
    int w=e->width,h=e->height; glDisable(2929); glEnable(3042); glBlendFunc(770,771);
    dr(w/2,h/2,12,1.5,w,h,1,1,1,0.8); dr(w/2,h/2,1.5,12,w,h,1,1,1,0.8);
    dr(130,h-130,32,32,w,h,0,0,0,0.6); dr(w-130,h-130,32,32,w,h,0,0,0,0.6);
    for(int i=0;i<9;i++){float sx=(w-450)/2+i*50+25;dr(sx,h-50,22,22,w,h,0.1,0.1,0.1,0.6);if(i==e->selectedSlot){dr(sx,h-72,22,2,w,h,1,1,1,1);dr(sx,h-28,22,2,w,h,1,1,1,1);dr(sx-22,h-50,2,22,w,h,1,1,1,1);dr(sx+22,h-50,2,22,w,h,1,1,1,1);}}
}
void draw_menu(struct engine* e){glClearColor(0,0,0,1);glClear(16384);dr(e->width/2,e->height/2,100,40,e->width,e->height,1,1,1,1);}
#endif
