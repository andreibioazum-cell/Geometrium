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
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

unsigned int game_seed = 0;

static void engine_draw_frame(struct engine* eng) {
    if (!eng->display || !eng->surface || eng->program == 0) return;
    
    eglQuerySurface(eng->display, eng->surface, EGL_WIDTH, &eng->width);
    eglQuerySurface(eng->display, eng->surface, EGL_HEIGHT, &eng->height);
    glViewport(0, 0, eng->width, eng->height);

    if (eng->gameState == STATE_MENU) {
        glClearColor(0.2f, 0.6f, 0.3f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        draw_menu(eng);
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

static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) {
        eng->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(eng->display, NULL, NULL);

        EGLConfig config; EGLint n;
        EGLint att[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_DEPTH_SIZE, 16, EGL_NONE };
        eglChooseConfig(eng->display, att, &config, 1, &n);
        eng->surface = eglCreateWindowSurface(eng->display, config, app->window, NULL);
        
        EGLint ctxAtt[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        eng->context = eglCreateContext(eng->display, config, NULL, ctxAtt);
        eglMakeCurrent(eng->display, eng->surface, eng->surface, eng->context);

        // УПРОЩЕННЫЙ ШЕЙДЕР (БЕЗ INT)
        const char* vS = 
            "attribute vec3 pos; attribute vec2 uv; attribute vec3 norm; attribute float type;"
            "varying vec2 vUV; varying vec3 vNorm; varying float vType;"
            "uniform mat4 m;"
            "void main(){ gl_Position=m*vec4(pos,1.0); vUV=uv; vNorm=norm; vType=type; }";

        const char* fS = 
            "precision mediump float;"
            "varying vec2 vUV; varying vec3 vNorm; varying float vType;"
            "uniform sampler2D texTop, texSide, texDown, texLeaves, texTreeSide, texTreeTop;"
            "void main(){"
            "  vec4 tc;"
            "  if(vType > 2.5){ tc = texture2D(texLeaves, vUV); if(tc.a < 0.5) discard; }"
            "  else if(vType > 1.5){"
            "    if(vNorm.y > 0.5 || vNorm.y < -0.5) tc = texture2D(texTreeTop, vUV);"
            "    else tc = texture2D(texTreeSide, vUV);"
            "  } else {"
            "    if(vNorm.y > 0.5) tc = texture2D(texTop, vUV);"
            "    else if(vNorm.y < -0.5) tc = texture2D(texDown, vUV);"
            "    else tc = texture2D(texSide, vUV);"
            "  }"
            "  float light = clamp(dot(vNorm, vec3(0.4, 1.0, 0.3)) * 0.6 + 0.5, 0.4, 1.0);"
            "  gl_FragColor = vec4(tc.rgb * light, tc.a);"
            "}";

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vS, NULL); glCompileShader(vs);
        
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fS, NULL); glCompileShader(fs);

        eng->program = glCreateProgram();
        glAttachShader(eng->program, vs);
        glAttachShader(eng->program, fs);
        
        // Принудительная привязка до линковки
        glBindAttribLocation(eng->program, 0, "pos");
        glBindAttribLocation(eng->program, 1, "uv");
        glBindAttribLocation(eng->program, 2, "norm");
        glBindAttribLocation(eng->program, 3, "type");
        
        glLinkProgram(eng->program);

        init_textures(eng);
        init_ui_shader();
        
        srand(time(NULL));
        game_seed = (unsigned int)rand();
        eng->worldSeed = (int)game_seed;
    }
}

void android_main(struct android_app* state) {
    struct engine* eng = (struct engine*)malloc(sizeof(struct engine));
    if(!eng) return;
    memset(eng, 0, sizeof(struct engine));

    eng->app = state;
    eng->movePointerId = -1;
    eng->lookPointerId = -1;
    eng->gameState = STATE_MENU;

    for(int i=0; i<INV_SLOTS; i++) eng->invSlots[i] = BLOCK_AIR;
    eng->invSlots[0] = BLOCK_GRASS;
    eng->invSlots[1] = BLOCK_WOOD;
    eng->invSlots[2] = BLOCK_LEAVES;

    eng->camPos[1] = 20.0f;
    state->userData = eng;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;

    while (1) {
        int ev; struct android_poll_source* s;
        while (ALooper_pollOnce(0, NULL, &ev, (void**)&s) >= 0) {
            if (s) s->process(state, s);
            if (state->destroyRequested) { 
                if(eng->display) {
                    eglMakeCurrent(eng->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                    eglDestroyContext(eng->display, eng->context);
                    eglDestroySurface(eng->display, eng->surface);
                    eglTerminate(eng->display);
                }
                free(eng); 
                return; 
            }
        }
        engine_draw_frame(eng);
    }
}
