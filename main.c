#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/log.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#include "engine.h"
#include "world.h"
#include "math_utils.h"
#include "physics.h"
#include "input.h"
#include "render.h"

void copy_to_clipboard(struct android_app* app, const char* text) {
    JNIEnv* env = NULL;
    (*app->activity->vm)->AttachCurrentThread(app->activity->vm, &env, NULL);
    jclass ctx = (*env)->FindClass(env, "android/content/Context");
    jmethodID getSys = (*env)->GetMethodID(env, ctx, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jobject cb = (*env)->CallObjectMethod(env, app->activity->clazz, getSys, (*env)->NewStringUTF(env, "clipboard"));
    jclass cd = (*env)->FindClass(env, "android/content/ClipData");
    jmethodID newT = (*env)->GetStaticMethodID(env, cd, "newPlainText", "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");
    jobject clip = (*env)->CallStaticObjectMethod(env, cd, newT, (*env)->NewStringUTF(env, "Err"), (*env)->NewStringUTF(env, text));
    jclass cm = (*env)->FindClass(env, "android/content/ClipboardManager");
    (*env)->CallVoidMethod(env, cb, (*env)->GetMethodID(env, cm, "setPrimaryClip", "(Landroid/content/ClipData;)V"), clip);
    (*app->activity->vm)->DetachCurrentThread(app->activity->vm);
}

unsigned int game_seed = 0;
bool fatal_error = false;

static void engine_draw_frame(struct engine* eng) {
    if (!eng->display || !eng->surface) return;
    if (fatal_error) { glClearColor(1,0,0,1); glClear(GL_COLOR_BUFFER_BIT); eglSwapBuffers(eng->display, eng->surface); return; }
    eglQuerySurface(eng->display, eng->surface, EGL_WIDTH, &eng->width);
    eglQuerySurface(eng->display, eng->surface, EGL_HEIGHT, &eng->height);
    glViewport(0, 0, eng->width, eng->height);
    if (eng->gameState == STATE_MENU) {
        glClearColor(0.2f, 0.6f, 0.3f, 1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        draw_menu(eng); eglSwapBuffers(eng->display, eng->surface); return;
    }
    update_world(eng); apply_physics(eng);
    if(eng->isBreaking) break_block(eng);
    glClearColor(0.53f, 0.81f, 0.98f, 1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    render_world(eng); draw_ui(eng);
    eglSwapBuffers(eng->display, eng->surface);
}

static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) {
        eng->display = eglGetDisplay(EGL_DEFAULT_DISPLAY); eglInitialize(eng->display, NULL, NULL);
        EGLConfig config; EGLint n; EGLint att[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_DEPTH_SIZE, 16, EGL_NONE };
        eglChooseConfig(eng->display, att, &config, 1, &n);
        eng->surface = eglCreateWindowSurface(eng->display, config, app->window, NULL);
        eng->context = eglCreateContext(eng->display, config, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
        if (!eglMakeCurrent(eng->display, eng->surface, eng->surface, eng->context)) { fatal_error=true; copy_to_clipboard(app, "EGL Fail"); return; }

        const char* vS = "attribute vec3 pos; attribute vec2 uv; attribute vec3 norm; attribute float type; varying vec2 vUV; varying float vL; varying float vT; varying float vNY; uniform mat4 m; void main(){ gl_Position=m*vec4(pos,1.0); vUV=uv; vT=type; vNY=norm.y; vL=clamp(dot(norm, vec3(0.4,1.0,0.3)), 0.4, 1.0); }";
        const char* fS = "precision mediump float; varying vec2 vUV; varying float vL; varying float vT; varying float vNY; uniform sampler2D uT1, uT2, uT3, uT4, uT5, uT6; void main(){ vec4 c; if(vT > 2.5){ c=texture2D(uT4, vUV); if(c.a < 0.1) discard; } else if(vT > 1.5){ if(vNY>0.5 || vNY<-0.5) c=texture2D(uT6, vUV); else c=texture2D(uT5, vUV); } else { if(vNY>0.5) c=texture2D(uT1, vUV); else if(vNY<-0.5) c=texture2D(uT3, vUV); else c=texture2D(uT2, vUV); } gl_FragColor=vec4(c.rgb*vL, c.a); }";

        GLuint vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs, 1, &vS, NULL); glCompileShader(vs);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs, 1, &fS, NULL); glCompileShader(fs);
        eng->program = glCreateProgram(); glAttachShader(eng->program, vs); glAttachShader(eng->program, fs);
        glBindAttribLocation(eng->program, 0, "pos"); glBindAttribLocation(eng->program, 1, "uv");
        glBindAttribLocation(eng->program, 2, "norm"); glBindAttribLocation(eng->program, 3, "type");
        glLinkProgram(eng->program);
        init_textures(eng); init_ui_shader(); game_seed = (unsigned int)time(NULL);
    }
}

void android_main(struct android_app* state) {
    struct engine* eng = (struct engine*)malloc(sizeof(struct engine));
    memset(eng, 0, sizeof(struct engine));
    eng->app = state; eng->gameState = STATE_MENU;
    eng->invSlots[0]=BLOCK_GRASS; eng->invSlots[1]=BLOCK_WOOD; eng->invSlots[2]=BLOCK_LEAVES;
    eng->camPos[1]=20.0f; state->userData = eng; state->onAppCmd = engine_handle_cmd; state->onInputEvent = engine_handle_input;
    while (1) {
        int ev; struct android_poll_source* s;
        while (ALooper_pollOnce(0, NULL, &ev, (void**)&s) >= 0) { if (s) s->process(state, s); if (state->destroyRequested) { free(eng); return; } }
        engine_draw_frame(eng);
    }
}
