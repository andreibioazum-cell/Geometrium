#include <android/log.h>
#include "engine.h"
#include "render.h"
#include "physics.h"
#include "input.h"
#include "world.h"
#include "mod_loader.h"

#define LOG_TAG "Geometrium"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static void init_egl(struct engine* eng) {
    const EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    eng->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(eng->display, NULL, NULL);

    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(eng->display, attribs, &config, 1, &numConfigs);

    EGLint format;
    eglGetConfigAttrib(eng->display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(eng->app->window, 0, 0, format);

    eng->surface = eglCreateWindowSurface(eng->display, config, eng->app->window, NULL);
    eng->context = eglCreateContext(eng->display, config, EGL_NO_CONTEXT, context_attribs);
    eglMakeCurrent(eng->display, eng->surface, eng->surface, eng->context);

    eglQuerySurface(eng->display, eng->surface, EGL_WIDTH, &eng->width);
    eglQuerySurface(eng->display, eng->surface, EGL_HEIGHT, &eng->height);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.5f, 0.7f, 1.0f, 1.0f);

    LOGI("EGL initialized: %dx%d", eng->width, eng->height);
}

static void destroy_egl(struct engine* eng) {
    if (eng->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(eng->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (eng->context != EGL_NO_CONTEXT) eglDestroyContext(eng->display, eng->context);
        if (eng->surface != EGL_NO_SURFACE) eglDestroySurface(eng->display, eng->surface);
        eglTerminate(eng->display);
    }
    eng->display = EGL_NO_DISPLAY;
    eng->context = EGL_NO_CONTEXT;
    eng->surface = EGL_NO_SURFACE;
}

static void init_gl(struct engine* eng) {
    init_world_shader(eng);
    init_ui_shader(eng);
    
    eng->texGrassTop  = load_tex(eng, "grass_top.png");
    eng->texGrassSide = load_tex(eng, "grass_side.png");
    eng->texGrassDown = load_tex(eng, "grass_bottom.png");

    glGenBuffers(1, &eng->vbo);
    
    LOGI("GL initialized, textures loaded");
}

static void handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (eng->app->window) {
                init_egl(eng);
                init_gl(eng);
                mod_load_all(eng);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            mod_unload_all(eng);
            destroy_egl(eng);
            break;
        case APP_CMD_GAINED_FOCUS:
            LOGI("Focus gained");
            break;
        case APP_CMD_LOST_FOCUS:
            LOGI("Focus lost");
            break;
    }
}

void android_main(struct android_app* state) {
    struct engine eng = {0};
    
    state->userData = &eng;
    state->onAppCmd = handle_cmd;
    state->onInputEvent = engine_handle_input;
    
    eng.app = state;
    eng.display = EGL_NO_DISPLAY;
    eng.context = EGL_NO_CONTEXT;
    eng.surface = EGL_NO_SURFACE;
    eng.movePointerId = -1;
    eng.lookPointerId = -1;
    eng.camPos[1] = 20.0f;
    eng.gameState = STATE_PLAYING;
    eng.worldSeed = 12345;
    eng.invSlots[0] = BLOCK_GRASS;
    eng.selectedSlot = 0;
    eng.modLoader.nextBlockId = CUSTOM_BLOCK_START;

    while (1) {
        int events;
        struct android_poll_source* source;
        
        while (ALooper_pollOnce(0, NULL, &events, (void**)&source) >= 0) {
            if (source) source->process(state, source);
            if (state->destroyRequested) {
                destroy_egl(&eng);
                return;
            }
        }

        if (eng.display != EGL_NO_DISPLAY) {
            update_world(&eng);
            apply_physics(&eng);
            
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            render_world(&eng);
            render_ui(&eng);
            
            eglSwapBuffers(eng.display, eng.surface);
        }
    }
}
