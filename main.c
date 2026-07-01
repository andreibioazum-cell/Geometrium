#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/log.h>

#include "engine.h"
#include "world.h"
#include "math_utils.h"
#include "physics.h"
#include "input.h"
#include "render.h"

#define LOG_TAG "Geometrium"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

unsigned int game_seed = 0;

static int check_shader(GLuint shader, const char* name) {
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        LOGE("Shader %s compile error: %s", name, log);
        return 0;
    }
    return 1;
}

static int check_program(GLuint prog, const char* name) {
    GLint status;
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (!status) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        LOGE("Program %s link error: %s", name, log);
        return 0;
    }
    return 1;
}

static void engine_draw_frame(struct engine* eng) {
    if (!eng->display) return;

    eglQuerySurface(eng->display, eng->surface, EGL_WIDTH, &eng->width);
    eglQuerySurface(eng->display, eng->surface, EGL_HEIGHT, &eng->height);
    glViewport(0, 0, eng->width, eng->height);

    if (eng->gameState == STATE_MENU) {
        glClearColor(0.2f, 0.6f, 0.3f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        draw_menu(eng);
        glDisable(GL_BLEND);
        eglSwapBuffers(eng->display, eng->surface);
        return;
    }

    update_world(eng);
    apply_physics(eng);
    glClearColor(0.53f, 0.81f, 0.98f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    render_world(eng);
    draw_ui(eng);
    eglSwapBuffers(eng->display, eng->surface);
}

static void init_inv_shader(struct engine* eng) {
    const char* vS =
        "attribute vec3 aPos;attribute vec2 aUV;attribute float aFace;"
        "varying vec2 vUV;varying float vFace;"
        "uniform vec2 offset;uniform float scale;uniform float aspect;"
        "void main(){"
        "  float ix=(aPos.x-aPos.z)*0.7071;"
        "  float iy=(aPos.x+aPos.z)*0.4082+aPos.y*0.8165;"
        "  gl_Position=vec4(ix*scale/aspect+offset.x,iy*scale+offset.y,0.0,1.0);"
        "  vUV=aUV;vFace=aFace;}";
    const char* fS =
        "precision mediump float;"
        "varying vec2 vUV;varying float vFace;"
        "uniform sampler2D texTop,texSide;"
        "void main(){"
        "  vec4 col;int f=int(vFace+0.5);"
        "  if(f==0)col=texture2D(texTop,vUV);else col=texture2D(texSide,vUV);"
        "  float l=1.0;if(f==1)l=0.7;if(f==2)l=0.85;"
        "  gl_FragColor=vec4(col.rgb*l,1.0);}";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vS, NULL);
    glCompileShader(vs);
    check_shader(vs, "inv_vs");
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fS, NULL);
    glCompileShader(fs);
    check_shader(fs, "inv_fs");
    eng->invProgram = glCreateProgram();
    glBindAttribLocation(eng->invProgram, 0, "aPos");
    glBindAttribLocation(eng->invProgram, 1, "aUV");
    glBindAttribLocation(eng->invProgram, 2, "aFace");
    glAttachShader(eng->invProgram, vs);
    glAttachShader(eng->invProgram, fs);
    glLinkProgram(eng->invProgram);
    check_program(eng->invProgram, "inv_prog");
    glDeleteShader(vs);
    glDeleteShader(fs);
}

static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) {
        eng->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (eng->display == EGL_NO_DISPLAY) {
            LOGE("eglGetDisplay failed");
            return;
        }
        if (!eglInitialize(eng->display, NULL, NULL)) {
            LOGE("eglInitialize failed");
            return;
        }

        EGLConfig config;
        EGLint n;
        EGLint att[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                         EGL_DEPTH_SIZE, 16,
                         EGL_NONE };
        if (!eglChooseConfig(eng->display, att, &config, 1, &n) || n == 0) {
            LOGE("eglChooseConfig failed");
            return;
        }

        eng->surface = eglCreateWindowSurface(eng->display, config, eng->app->window, NULL);
        if (eng->surface == EGL_NO_SURFACE) {
            LOGE("eglCreateWindowSurface failed");
            return;
        }

        EGLint ctxAtt[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        eng->context = eglCreateContext(eng->display, config, NULL, ctxAtt);
        if (eng->context == EGL_NO_CONTEXT) {
            LOGE("eglCreateContext failed");
            return;
        }

        if (!eglMakeCurrent(eng->display, eng->surface, eng->surface, eng->context)) {
            LOGE("eglMakeCurrent failed");
            return;
        }

        const char* vS =
            "attribute vec3 pos;attribute vec2 uv;attribute vec3 norm;"
            "varying vec2 vUV;varying vec3 vNorm;varying vec3 vWorldPos;"
            "uniform mat4 m;"
            "void main(){gl_Position=m*vec4(pos,1.0);vUV=uv;vNorm=norm;vWorldPos=pos;}";
        const char* fS =
            "precision mediump float;"
            "varying vec2 vUV;varying vec3 vNorm;varying vec3 vWorldPos;"
            "uniform vec3 camPos;uniform sampler2D texTop,texSide,texDown;"
            "void main(){"
            "  vec4 tc;if(vNorm.y>0.5)tc=texture2D(texTop,vUV);"
            "  else if(vNorm.y<-0.5)tc=texture2D(texDown,vUV);"
            "  else tc=texture2D(texSide,vUV);"
            "  vec3 sun=normalize(vec3(0.35,0.85,0.4));"
            "  float diff=max(dot(vNorm,sun),0.0),amb=0.42,fb=0.0;"
            "  if(vNorm.y>0.5)fb=0.12;if(vNorm.y<-0.5)fb=-0.18;"
            "  if(abs(vNorm.x)>0.5)fb=-0.06;if(abs(vNorm.z)>0.5)fb=-0.12;"
            "  float light=clamp(amb+diff*0.58+fb,0.18,1.0);"
            "  float rim=1.0-max(dot(vNorm,vec3(0,1,0)),0.0);rim=rim*rim*0.08;"
            "  vec3 lit=tc.rgb*light+rim;"
            "  float gray=dot(lit,vec3(0.299,0.587,0.114));"
            "  vec3 sat=mix(vec3(gray),lit,1.15);"
            "  float dist=length((vWorldPos-camPos).xz);"
            "  float fog=clamp((dist-22.0)/24.0,0.0,0.88);"
            "  gl_FragColor=vec4(mix(sat,vec3(0.53,0.81,0.98),fog),1.0);}";

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vS, NULL);
        glCompileShader(vs);
        check_shader(vs, "main_vs");
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fS, NULL);
        glCompileShader(fs);
        check_shader(fs, "main_fs");
        eng->program = glCreateProgram();
        glBindAttribLocation(eng->program, 0, "pos");
        glBindAttribLocation(eng->program, 1, "uv");
        glBindAttribLocation(eng->program, 2, "norm");
        glAttachShader(eng->program, vs);
        glAttachShader(eng->program, fs);
        glLinkProgram(eng->program);
        check_program(eng->program, "main_prog");
        glDeleteShader(vs);
        glDeleteShader(fs);

        init_textures(eng);
        init_ui_shader();
        init_inv_shader(eng);
    }
}

void android_main(struct android_app* state) {
    struct engine eng = {0};
    eng.movePointerId = -1;
    eng.lookPointerId = -1;
    eng.worldLoaded = false;
    eng.meshDirty = true;
    eng.selectedSlot = 0;
    eng.gameState = STATE_MENU;
    eng.seedCursor = 0;
    eng.joyTouched = false;
    eng.invSlots[0] = BLOCK_GRASS;
    eng.invSlots[1] = BLOCK_GRASS;
    eng.invSlots[2] = BLOCK_GRASS;
    eng.camPos[0] = 0.5f;
    eng.camPos[2] = -0.5f;
    eng.camPos[1] = 10.0f;
    state->userData = &eng;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;
    eng.app = state;

    while (1) {
        int ev;
        struct android_poll_source* s;
        while (ALooper_pollOnce(0, NULL, &ev, (void**)&s) >= 0) {
            if (s) s->process(state, s);
            if (state->destroyRequested) return;
        }
        engine_draw_frame(&eng);
    }
}
