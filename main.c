#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "engine.h"
#include "world.h"
#include "math_utils.h"
#include "physics.h"
#include "input.h"
#include "render.h"

static void engine_draw_frame(struct engine* eng) {
    if (!eng->display) return;
    update_world(eng); apply_physics(eng);
    eglQuerySurface(eng->display,eng->surface,EGL_WIDTH,&eng->width);
    eglQuerySurface(eng->display,eng->surface,EGL_HEIGHT,&eng->height);
    glViewport(0,0,eng->width,eng->height);
    glClearColor(0.53f,0.81f,0.98f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    render_world(eng); draw_ui(eng);
    eglSwapBuffers(eng->display,eng->surface);
}

static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng=(struct engine*)app->userData;
    if(cmd==APP_CMD_INIT_WINDOW){
        eng->display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(eng->display,0,0);
        EGLConfig config;EGLint n;
        EGLint att[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_DEPTH_SIZE,16,EGL_NONE};
        eglChooseConfig(eng->display,att,&config,1,&n);
        eng->surface=eglCreateWindowSurface(eng->display,config,eng->app->window,NULL);
        EGLint ctxAtt[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
        eng->context=eglCreateContext(eng->display,config,NULL,ctxAtt);
        eglMakeCurrent(eng->display,eng->surface,eng->surface,eng->context);

        const char*vS=
            "attribute vec3 pos;attribute vec2 uv;attribute vec3 norm;"
            "varying vec2 vUV;varying vec3 vNorm;varying vec3 vWorldPos;"
            "uniform mat4 m;"
            "void main(){gl_Position=m*vec4(pos,1.0);vUV=uv;vNorm=norm;vWorldPos=pos;}";

        const char*fS=
            "precision mediump float;"
            "varying vec2 vUV;varying vec3 vNorm;varying vec3 vWorldPos;"
            "uniform vec3 camPos;"
            "uniform sampler2D texTop,texSide,texDown;"
            "void main(){"
            "  vec4 tc;"
            "  if(vNorm.y>0.5)tc=texture2D(texTop,vUV);"
            "  else if(vNorm.y<-0.5)tc=texture2D(texDown,vUV);"
            "  else tc=texture2D(texSide,vUV);"
            "  vec3 sun=normalize(vec3(0.35,0.85,0.4));"
            "  float diff=max(dot(vNorm,sun),0.0);"
            "  float amb=0.42;"
            "  float fb=0.0;"
            "  if(vNorm.y>0.5)fb=0.12;"
            "  if(vNorm.y<-0.5)fb=-0.18;"
            "  if(abs(vNorm.x)>0.5)fb=-0.06;"
            "  if(abs(vNorm.z)>0.5)fb=-0.12;"
            "  float light=clamp(amb+diff*0.58+fb,0.18,1.0);"
            "  float rim=(1.0-max(dot(vNorm,vec3(0,1,0)),0.0));rim=rim*rim*0.08;"
            "  vec3 lit=tc.rgb*light+rim;"
            "  float gray=dot(lit,vec3(0.299,0.587,0.114));"
            "  vec3 sat=mix(vec3(gray),lit,1.15);"
            "  vec3 tf=vWorldPos-camPos;float dist=length(tf.xz);"
            "  vec3 fogC=vec3(0.53,0.81,0.98);"
            "  float fog=clamp((dist-22.0)/24.0,0.0,0.88);"
            "  gl_FragColor=vec4(mix(sat,fogC,fog),1.0);"
            "}";

        GLuint vs=glCreateShader(GL_VERTEX_SHADER);glShaderSource(vs,1,&vS,NULL);glCompileShader(vs);
        GLuint fs=glCreateShader(GL_FRAGMENT_SHADER);glShaderSource(fs,1,&fS,NULL);glCompileShader(fs);
        eng->program=glCreateProgram();
        glBindAttribLocation(eng->program,0,"pos");
        glBindAttribLocation(eng->program,1,"uv");
        glBindAttribLocation(eng->program,2,"norm");
        glAttachShader(eng->program,vs);glAttachShader(eng->program,fs);
        glLinkProgram(eng->program);glDeleteShader(vs);glDeleteShader(fs);
        init_textures(eng);init_ui_shader();
    }
}

void android_main(struct android_app* state) {
    struct engine eng={0};
    eng.movePointerId=-1; eng.lookPointerId=-1;
    eng.worldLoaded=false; eng.meshDirty=true; eng.selectedSlot=0;
    /* Начальный инвентарь — 3 блока травы */
    eng.invSlots[0]=BLOCK_GRASS; eng.invSlots[1]=BLOCK_GRASS; eng.invSlots[2]=BLOCK_GRASS;
    eng.camPos[0]=0.5f; eng.camPos[2]=-0.5f;
    eng.camPos[1]=(float)get_height(0,0)+2.5f;
    state->userData=&eng; state->onAppCmd=engine_handle_cmd;
    state->onInputEvent=engine_handle_input; eng.app=state;
    while(1){
        int ev;struct android_poll_source*s;
        while(ALooper_pollOnce(0,NULL,&ev,(void**)&s)>=0){
            if(s)s->process(state,s);if(state->destroyRequested)return;}
        engine_draw_frame(&eng);
    }
}
