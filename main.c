#include "engine.h"
#include "world.h"
#include "math_utils.h"
#include "physics.h"
#include "input.h"
#include "render.h"
#include <time.h>
unsigned int game_seed;
static void draw(struct engine* e) {
    if(!e->display||!e->surface)return; eglQuerySurface(e->display,e->surface,12375,&e->width); eglQuerySurface(e->display,e->surface,12374,&e->height);
    glViewport(0,0,e->width,e->height); if(e->gameState==0){draw_menu(e);eglSwapBuffers(e->display,e->surface);return;}
    update_world(e); apply_physics(e); if(e->isBreaking)break_block(e);
    glClearColor(0.5,0.8,0.9,1); glClear(16640); render_world(e); draw_ui(e); eglSwapBuffers(e->display,e->surface);
}
static void cmd(struct android_app* a, int32_t c) {
    struct engine* e=(struct engine*)a->userData; if(c==APP_CMD_INIT_WINDOW){
        e->display=eglGetDisplay(0); eglInitialize(e->display,0,0); EGLConfig cfg; EGLint n; eglChooseConfig(e->display,(EGLint[]){12352,4,12325,16,12344},&cfg,1,&n);
        e->surface=eglCreateWindowSurface(e->display,cfg,a->window,0); e->context=eglCreateContext(e->display,cfg,0,(EGLint[]){12440,2,12344}); eglMakeCurrent(e->display,e->surface,e->surface,e->context);
        const char *vs="attribute vec3 pos;attribute vec2 uv;attribute vec3 norm;attribute float type;varying vec2 vUV;varying float vL;varying float vT;varying float vNY;uniform mat4 m;void main(){gl_Position=m*vec4(pos,1.0);vUV=uv;vT=type;vNY=norm.y;vL=clamp(dot(norm,vec3(0.4,1.0,0.3)),0.4,1.0);}",
        *fs="precision mediump float;varying vec2 vUV;varying float vL;varying float vT;varying float vNY;uniform sampler2D uT1,uT2,uT3,uT4,uT5,uT6;void main(){vec4 c;if(vT>2.5){c=texture2D(uT4,vUV);if(c.a<0.1)discard;}else if(vT>1.5){c=(vNY>0.5||vNY<-0.5)?texture2D(uT6,vUV):texture2D(uT5,vUV);}else{c=(vNY>0.5)?texture2D(uT1,vUV):(vNY<-0.5?texture2D(uT3,vUV):texture2D(uT2,vUV));}gl_FragColor=vec4(c.rgb*vL,c.a);}";
        e->program=glCreateProgram(); GLuint s=glCreateShader(35633); glShaderSource(s,1,&vs,0); glCompileShader(s); glAttachShader(e->program,s); s=glCreateShader(35632); glShaderSource(s,1,&fs,0); glCompileShader(s); glAttachShader(e->program,s);
        glBindAttribLocation(e->program,0,"pos");glBindAttribLocation(e->program,1,"uv");glBindAttribLocation(e->program,2,"norm");glBindAttribLocation(e->program,3,"type");glLinkProgram(e->program);
        init_textures(e);init_ui_shader();game_seed=time(0);
    }
}
void android_main(struct android_app* s) {
    struct engine* e=(struct engine*)malloc(sizeof(struct engine)); memset(e,0,sizeof(struct engine));
    e->app=s;e->movePointerId=e->lookPointerId=-1;e->invSlots[0]=1;e->invSlots[1]=2;e->invSlots[2]=3;e->camPos[1]=15;
    s->userData=e;s->onAppCmd=cmd;s->onInputEvent=engine_handle_input;
    while(1){int ev;struct android_poll_source* src;while(ALooper_pollOnce(0,0,&ev,(void**)&src)>=0){if(src)src->process(s,src);if(s->destroyRequested){free(e);return;}}draw(e);}
}
