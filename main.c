#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/log.h>
#include <stdlib.h>
#include <time.h>

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

    update_world(eng);
    apply_physics(eng);
    glClearColor(0.53f, 0.81f, 0.98f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    render_world(eng);
    draw_ui(eng);
    eglSwapBuffers(eng->display, eng->surface);
}

static void init_inv_shader(struct engine* eng) {
    // Заглушка, т.к. не используется
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

        if (eng->seedCursor == 0) {
            srand(time(NULL));
            game_seed = (unsigned int)rand();
            eng->worldSeed = (int)game_seed;
        }
    }
}

void android_main(struct android_app* state) {
    struct engine eng = {0};
    eng.movePointerId = -1;
    eng.lookPointerId = -1;
    eng.worldLoaded = false;
    eng.meshDirty = true;
    eng.selectedSlot = 0;
    eng.gameState = STATE_PLAYING;
    eng.seedCursor = 0;
    eng.joyTouched = false;
    eng.vbo = 0;
    // Начальный инвентарь: трава, дерево, листва – все выглядят как трава
    eng.invSlots[0] = BLOCK_GRASS;
    eng.invSlots[1] = BLOCK_WOOD;
    eng.invSlots[2] = BLOCK_LEAVES;
    eng.invSlots[3] = BLOCK_GRASS;
    eng.invSlots[4] = BLOCK_GRASS;
    eng.invSlots[5] = BLOCK_GRASS;
    eng.invSlots[6] = BLOCK_GRASS;
    eng.invSlots[7] = BLOCK_GRASS;
    eng.invSlots[8] = BLOCK_GRASS;

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
