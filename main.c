#include "engine.h"
#include "math_utils.h"
#include "world.h"
#include "physics.h"
#include "render.h"

static void handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) {
        eng->display = eglGetDisplay(EGL_DEFAULT_DISPLAY); eglInitialize(eng->display, 0, 0);
        EGLConfig cfg; EGLint n; EGLint att[]={EGL_RENDERABLE_TYPE,4,EGL_DEPTH_SIZE,16,EGL_NONE};
        eglChooseConfig(eng->display, att, &cfg, 1, &n);
        eng->surface = eglCreateWindowSurface(eng->display, cfg, app->window, NULL);
        EGLint catt[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
        eng->context = eglCreateContext(eng->display, cfg, NULL, catt);
        eglMakeCurrent(eng->display, eng->surface, eng->surface, eng->context);
        
        const char* vs = "attribute vec4 pos; attribute vec2 uv; attribute vec3 norm; varying vec2 v_uv; varying vec3 v_n; uniform mat4 m; void main(){ gl_Position=m*pos; v_uv=uv; v_n=norm; }";
        const char* fs = "precision mediump float; varying vec2 v_uv; varying vec3 v_n; uniform sampler2D tT,tS,tD; void main(){ vec4 c; if(v_n.y>0.5)c=texture2D(tT,v_uv); else if(v_n.y<-0.5)c=texture2D(tD,v_uv); else c=texture2D(tS,v_uv); gl_FragColor=vec4(c.rgb*(abs(v_n.x)*0.8+abs(v_n.y)*1.0+abs(v_n.z)*0.9)*0.8,1.0); }";
        GLuint vsh=glCreateShader(35633); glShaderSource(vsh,1,&vs,0); glCompileShader(vsh);
        GLuint fsh=glCreateShader(35632); glShaderSource(fsh,1,&fs,0); glCompileShader(fsh);
        eng->program=glCreateProgram(); glAttachShader(eng->program,vsh); glAttachShader(eng->program,fsh); glLinkProgram(eng->program);
        glUseProgram(eng->program); glUniform1i(glGetUniformLocation(eng->program,"tT"),0); glUniform1i(glGetUniformLocation(eng->program,"tS"),1); glUniform1i(glGetUniformLocation(eng->program,"tD"),2);

        eng->uiProg=glCreateProgram();
        const char* uvs="attribute vec4 p; void main(){gl_Position=p;}";
        const char* ufs="precision mediump float; uniform vec4 col; void main(){gl_FragColor=col;}";
        GLuint uvsh=glCreateShader(35633); glShaderSource(uvsh,1,&uvs,0); glCompileShader(uvsh);
        GLuint ufsh=glCreateShader(35632); glShaderSource(ufsh,1,&ufs,0); glCompileShader(ufsh);
        glAttachShader(eng->uiProg,uvsh); glAttachShader(eng->uiProg,ufsh); glLinkProgram(eng->uiProg);

        eng->texGrassTop=load_tex(app,"grass_top.png"); eng->texGrassSide=load_tex(app,"grass_side.png"); eng->texGrassDown=load_tex(app,"grass_down.png");
    }
}

static int32_t handle_input(struct android_app* app, AInputEvent* ev) {
    struct engine* eng = app->userData;
    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_MOTION) {
        int a = AMotionEvent_getAction(ev), id = AMotionEvent_getPointerId(ev, (a & 0xff00) >> 8);
        float x = AMotionEvent_getX(ev, (a & 0xff00) >> 8), y = AMotionEvent_getY(ev, (a & 0xff00) >> 8);
        int type = a & 0xff;
        if (type == 0 || type == 5) {
            if (eng->gameState == STATE_MENU) { if(x > eng->width/2+50) eng->gameState=STATE_PLAYING; else if(eng->seedCursor<6) eng->seedDigits[eng->seedCursor++]=(int)(x/100)%10; }
            else {
                if(x > eng->width-150 && y < 150) { int hx,hy,hz,px,py,pz; if(raycast(eng,&hx,&hy,&hz,&px,&py,&pz)) world_set_block(eng,hx,hy,hz,0); }
                else if(x > eng->width-150 && y > 150 && y < 300) { int hx,hy,hz,px,py,pz; if(raycast(eng,&hx,&hy,&hz,&px,&py,&pz) && py!=-999) world_set_block(eng,px,py,pz,1); }
                else if(x < eng->width/2) { eng->movePointerId=id; eng->isMoving=true; eng->joyX=x; eng->joyY=y; }
                else { eng->lookPointerId=id; eng->lastTouchX=x; eng->lastTouchY=y; }
            }
        } else if (type == 1 || type == 6) {
            if (id == eng->movePointerId) { eng->isMoving=false; eng->movePointerId=-1; }
            if (id == eng->lookPointerId) eng->lookPointerId=-1;
        } else if (type == 2) {
            for(int i=0; i<AMotionEvent_getPointerCount(ev); i++) {
                int pid = AMotionEvent_getPointerId(ev, i); float px = AMotionEvent_getX(ev, i), py = AMotionEvent_getY(ev, i);
                if (pid == eng->movePointerId) { float dx=px-eng->joyX, dy=py-eng->joyY; float d=sqrtf(dx*dx+dy*dy); if(d>5){ eng->moveDirX=dx/d; eng->moveDirZ=dy/d; } }
                else if (pid == eng->lookPointerId) { eng->camRot[1]+=(px-eng->lastTouchX)*0.005f; eng->camRot[0]+=(py-eng->lastTouchY)*0.005f; eng->lastTouchX=px; eng->lastTouchY=py; }
            }
        }
        return 1;
    }
    return 0;
}

void android_main(struct android_app* state) {
    struct engine eng = {0}; eng.app = state; eng.camPos[1]=12; eng.movePointerId=-1; eng.lookPointerId=-1;
    state->userData = &eng; state->onAppCmd = handle_cmd; state->onInputEvent = handle_input;
    while (1) {
        int id, ev; struct android_poll_source* s;
        while ((id = ALooper_pollOnce(0, NULL, &ev, (void**)&s)) >= 0) { if(s) s->process(state, s); if(state->destroyRequested) return; }
        if (eng.display) {
            if (!eng.worldLoaded) load_blocks_around(&eng, 0, 0);
            if (eng.gameState == STATE_PLAYING) apply_physics(&eng);
            glClearColor(0.5, 0.8, 1, 1); glClear(16640);
            draw_world(&eng); draw_ui(&eng); eglSwapBuffers(eng.display, eng.surface);
        }
    }
}
