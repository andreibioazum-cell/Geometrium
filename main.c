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

    // Игровая логика
    update_world(eng);
    apply_physics(eng);

    // Отрисовка
    glClearColor(0.53f, 0.81f, 0.98f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    render_world(eng);
    draw_ui(eng);

    eglSwapBuffers(eng->display, eng->surface);
}

static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) {
        eng->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(eng->display, NULL, NULL);

        EGLConfig config;
        EGLint n;
        EGLint att[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                         EGL_DEPTH_SIZE, 16,
                         EGL_NONE };
        eglChooseConfig(eng->display, att, &config, 1, &n);
        eng->surface = eglCreateWindowSurface(eng->display, config, eng->app->window, NULL);
        
        EGLint ctxAtt[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        eng->context = eglCreateContext(eng->display, config, NULL, ctxAtt);
        eglMakeCurrent(eng->display, eng->surface, eng->surface, eng->context);

        // --- ШЕЙДЕР МИРА ---
        const char* vS =
            "attribute vec3 pos; attribute vec2 uv; attribute vec3 norm;"
            "varying vec2 vUV; varying vec3 vNorm; varying vec3 vWorldPos;"
            "uniform mat4 m;"
            "void main(){ gl_Position=m*vec4(pos,1.0); vUV=uv; vNorm=norm; vWorldPos=pos; }";

        const char* fS =
            "precision mediump float;"
            "varying vec2 vUV; varying vec3 vNorm; varying vec3 vWorldPos;"
            "uniform vec3 camPos;"
            "uniform sampler2D texTop, texSide, texDown;"
            "void main(){"
            "  vec4 tc;"
            "  if(vNorm.y > 0.5) tc = texture2D(texTop, vUV);"
            "  else if(vNorm.y < -0.5) tc = texture2D(texDown, vUV);"
            "  else tc = texture2D(texSide, vUV);"
            "  vec3 sun = normalize(vec3(0.35, 0.85, 0.4));"
            "  float diff = max(dot(vNorm, sun), 0.0), amb = 0.45;"
            "  float fb = 0.0;"
            "  if(abs(vNorm.x)>0.5) fb=-0.1; if(abs(vNorm.z)>0.5) fb=-0.15;"
            "  float light = clamp(amb + diff * 0.55 + fb, 0.2, 1.0);"
            "  float dist = length((vWorldPos - camPos).xz);"
            "  float fog = clamp((dist - 25.0)/20.0, 0.0, 0.9);"
            "  gl_FragColor = vec4(mix(tc.rgb * light, vec3(0.53, 0.81, 0.98), fog), 1.0);"
            "}";

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vS, NULL); glCompileShader(vs);
        check_shader(vs, "main_vs");

        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fS, NULL); glCompileShader(fs);
        check_shader(fs, "main_fs");

        eng->program = glCreateProgram();
        glAttachShader(eng->program, vs);
        glAttachShader(eng->program, fs);
        glBindAttribLocation(eng->program, 0, "pos");
        glBindAttribLocation(eng->program, 1, "uv");
        glBindAttribLocation(eng->program, 2, "norm");
        glLinkProgram(eng->program);
        check_program(eng->program, "main_prog");

        init_textures(eng);
        init_ui_shader();
        
        srand(time(NULL));
        game_seed = (unsigned int)rand();
        eng->worldSeed = (int)game_seed;
    }
}

void android_main(struct android_app* state) {
    struct engine eng = {0};
    eng.app = state;
    eng.movePointerId = -1;
    eng.lookPointerId = -1;
    eng.worldLoaded = false;
    eng.meshDirty = true;
    eng.selectedSlot = 0;
    eng.gameState = STATE_MENU;

    // Начальный инвентарь (теперь ограниченный)
    for(int i=0; i<INV_SLOTS; i++) eng.invSlots[i] = BLOCK_AIR;
    eng.invSlots[0] = BLOCK_GRASS;
    eng.invSlots[1] = BLOCK_WOOD;
    eng.invSlots[2] = BLOCK_LEAVES;

    eng.camPos[0] = 0.5f;
    eng.camPos[1] = 15.0f;
    eng.camPos[2] = -0.5f;

    state->userData = &eng;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;

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
