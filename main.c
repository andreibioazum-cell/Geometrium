#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdbool.h>
#include "cube.h"
#include "math_utils.h"
#include "world.h"

struct engine {
    struct android_app* app;
    EGLDisplay display; EGLSurface surface; EGLContext context;
    int32_t width, height;
    GLuint program;
    float camPos[3], camRot[2];
    float touchX, touchY;
    bool touching;
};

static void init_shaders(struct engine* eng) {
    const char* vS = "attribute vec4 p; attribute vec2 t; varying vec2 vT; uniform mat4 m; void main(){ gl_Position=m*p; vT=t; }";
    const char* fS = "precision mediump float; varying vec2 vT; void main(){ gl_FragColor=vec4(vT.x, vT.y + 0.3, 0.4, 1.0); }";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs, 1, &vS, NULL); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs, 1, &fS, NULL); glCompileShader(fs);
    eng->program = glCreateProgram();
    glAttachShader(eng->program, vs); glAttachShader(eng->program, fs);
    glLinkProgram(eng->program);
}

static int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        float x = AMotionEvent_getX(event, 0);
        float y = AMotionEvent_getY(event, 0);
        int action = AMotionEvent_getAction(event);
        if (action == AMOTION_EVENT_ACTION_DOWN) eng->touching = true;
        else if (action == AMOTION_EVENT_ACTION_UP) eng->touching = false;

        if (x < eng->width / 2) {
            eng->camPos[0] += sinf(eng->camRot[1]) * 0.2f;
            eng->camPos[2] -= cosf(eng->camRot[1]) * 0.2f;
        } else {
            eng->camRot[1] += (x - eng->touchX) * 0.005f;
            eng->camRot[0] += (y - eng->touchY) * 0.005f;
        }
        eng->touchX = x; eng->touchY = y;
        return 1;
    }
    return 0;
}

static void engine_draw_frame(struct engine* eng) {
    if (!eng->display) return;
    eglQuerySurface(eng->display, eng->surface, EGL_WIDTH, &eng->width);
    eglQuerySurface(eng->display, eng->surface, EGL_HEIGHT, &eng->height);
    glViewport(0, 0, eng->width, eng->height);
    glClearColor(0.5f, 0.8f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(eng->program);
    GLint mLoc = glGetUniformLocation(eng->program, "m");

    float proj[16], view[16], model[16], mvp[16], temp[16];
    mat4_perspective(proj, 1.0f, (float)eng->width/eng->height, 0.1f, 100.0f);
    mat4_lookat(view, eng->camPos, eng->camRot[0], eng->camRot[1]);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*4, cube_vertices);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*4, &cube_vertices[3]);
    glEnableVertexAttribArray(1);

    for(int x=0; x<WORLD_SIZE; x++) {
        for(int y=0; y<CHUNK_H; y++) {
            for(int z=0; z<WORLD_SIZE; z++) {
                if(map[x][y][z]) {
                    mat4_translate(model, (float)x, (float)y, (float)-z);
                    mat4_mul(temp, proj, view);
                    mat4_mul(mvp, temp, model);
                    glUniformMatrix4fv(mLoc, 1, GL_FALSE, mvp);
                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }
            }
        }
    }
    eglSwapBuffers(eng->display, eng->surface);
}

static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) {
        eng->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(eng->display, 0, 0);
        EGLConfig config; EGLint n;
        EGLint att[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_DEPTH_SIZE, 16, EGL_NONE };
        eglChooseConfig(eng->display, att, &config, 1, &n);
        eng->surface = eglCreateWindowSurface(eng->display, config, eng->app->window, NULL);
        eng->context = eglCreateContext(eng->display, config, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
        eglMakeCurrent(eng->display, eng->surface, eng->surface, eng->context);
        init_shaders(eng);
    }
}

void android_main(struct android_app* state) {
    struct engine eng = {0};
    eng.camPos[1] = 4.0f; eng.camPos[2] = 5.0f;
    generate_world();
    state->userData = &eng;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;
    eng.app = state;
    while (1) {
        int id, ev; struct android_poll_source* s;
        while ((id = ALooper_pollOnce(0, NULL, &ev, (void**)&s)) >= 0) {
            if (s) s->process(state, s);
            if (state->destroyRequested) return;
        }
        engine_draw_frame(&eng);
    }
}
