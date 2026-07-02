#ifndef RENDER_H
#define RENDER_H
#include "engine.h"
#include "math_utils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static void init_ui_shader(struct engine* eng) {
    const char* vS = "attribute vec4 p; attribute vec2 u; varying vec2 v; void main(){ gl_Position=p; v=u; }";
    const char* fS = "precision mediump float; uniform vec4 c; uniform sampler2D t; uniform int s; varying vec2 v; "
                     "void main(){ if(s==1) gl_FragColor=c*texture2D(t,v); else gl_FragColor=c; }";
    GLuint vs=glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs,1,&vS,NULL); glCompileShader(vs);
    GLuint fs=glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs,1,&fS,NULL); glCompileShader(fs);
    eng->uiProgram=glCreateProgram(); glAttachShader(eng->uiProgram,vs); glAttachShader(eng->uiProgram,fs);
    glBindAttribLocation(eng->uiProgram,0,"p"); glBindAttribLocation(eng->uiProgram,1,"u");
    glLinkProgram(eng->uiProgram);
}

static void draw_rect(struct engine* eng, float cx, float cy, float hw, float hh, float r, float g, float b, float a) {
    float nx=(cx/eng->width)*2-1, ny=1-(cy/eng->height)*2, rw=hw/eng->width*2, rh=hh/eng->height*2;
    float v[] = {nx-rw,ny-rh, nx+rw,ny-rh, nx+rw,ny+rh, nx-rw,ny-rh, nx+rw,ny+rh, nx-rw,ny+rh};
    glUseProgram(eng->uiProgram); glUniform4f(glGetUniformLocation(eng->uiProgram,"c"),r,g,b,a);
    glUniform1i(glGetUniformLocation(eng->uiProgram,"s"),0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,v); glEnableVertexAttribArray(0); glDrawArrays(GL_TRIANGLES,0,6);
}

static void draw_circle(struct engine* eng, float cx, float cy, float r, float cr, float cg, float cb, float ca) {
    float nx=(cx/eng->width)*2-1, ny=1-(cy/eng->height)*2, rx=r/eng->width*2, ry=r/eng->height*2;
    float v[26*2]; v[0]=nx; v[1]=ny;
    for(int i=0; i<=24; i++) { float a=i/24.0f*2.0f*PI; v[(i+1)*2]=nx+cosf(a)*rx; v[(i+1)*2+1]=ny+sinf(a)*ry; }
    glUseProgram(eng->uiProgram); glUniform4f(glGetUniformLocation(eng->uiProgram,"c"),cr,cg,cb,ca);
    glUniform1i(glGetUniformLocation(eng->uiProgram,"s"),0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,v); glEnableVertexAttribArray(0); glDrawArrays(GL_TRIANGLE_FAN,0,26);
}

static void draw_ring(struct engine* eng, float cx, float cy, float r, float t, float cr, float cg, float cb, float ca) {
    float nx=(cx/eng->width)*2-1, ny=1-(cy/eng->height)*2, rxo=r/eng->width*2, ryo=r/eng->height*2, rxi=(r-t)/eng->width*2, ryi=(r-t)/eng->height*2;
    float v[33*4];
    for(int i=0; i<=32; i++){ float a=i/32.0f*2.0f*PI, c=cosf(a), s=sinf(a); v[i*4]=nx+c*rxo; v[i*4+1]=ny+s*ryo; v[i*4+2]=nx+c*rxi; v[i*4+3]=ny+s*ryi; }
    glUseProgram(eng->uiProgram); glUniform4f(glGetUniformLocation(eng->uiProgram,"c"),cr,cg,cb,ca);
    glUniform1i(glGetUniformLocation(eng->uiProgram,"s"),0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,v); glEnableVertexAttribArray(0); glDrawArrays(GL_TRIANGLE_STRIP,0,66);
}

// Заглушки для текстур и мира (реализация была в твоем коде)
static GLuint load_tex(struct engine* eng, const char* n) { 
    AAsset* a = AAssetManager_open(eng->app->activity->assetManager, n, AASSET_MODE_BUFFER);
    if(!a) return 0;
    size_t l = AAsset_getLength(a); unsigned char* b = malloc(l); AAsset_read(a,b,l); AAsset_close(a);
    int w,h,c; unsigned char* i = stbi_load_from_memory(b,l,&w,&h,&c,4); free(b);
    GLuint t; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,i);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    stbi_image_free(i); return t;
}
#endif
