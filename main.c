#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <math.h>
#include "cube.h"
#include "math_utils.h"

struct engine {
    struct android_app* app;
    EGLDisplay display;
    EGLContext context;
    EGLSurface surface;
    int32_t width;
    int32_t height;
    GLuint program;
};

// Простейшая матрица умножения
void mat4_mul(float* res, float* a, float* b) {
    float tmp[16];
    for (int i=0; i<4; i++) {
        for (int j=0; j<4; j++) {
            tmp[i*4+j] = a[i*4+0]*b[0*4+j] + a[i*4+1]*b[1*4+j] + a[i*4+2]*b[2*4+j] + a[i*4+3]*b[3*4+j];
        }
    }
    for (int i=0; i<16; i++) res[i] = tmp[i];
}

static int engine_init_display(struct engine* engine) {
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, 0, 0);

    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8,
        EGL_DEPTH_SIZE, 16, EGL_NONE
    };
    EGLConfig config; EGLint numConfigs;
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);
    
    EGLSurface surface = eglCreateWindowSurface(display, config, engine->app->window, NULL);
    EGLContext context = eglCreateContext(display, config, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) return -1;

    eglQuerySurface(display, surface, EGL_WIDTH, &engine->width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &engine->height);

    engine->display = display; engine->surface = surface; engine->context = context;

    // Шейдеры: добавили цвет от координат, чтобы видеть грани без текстур
    const char* vSrc = "attribute vec4 p; attribute vec2 t; varying vec2 vT; uniform mat4 m; void main() { gl_Position = m*p; vT = t; }";
    const char* fSrc = "precision mediump float; varying vec2 vT; void main() { gl_FragColor = vec4(vT, 1.0, 1.0); }";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs, 1, &vSrc, NULL); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs, 1, &fSrc, NULL); glCompileShader(fs);
    engine->program = glCreateProgram();
    glAttachShader(engine->program, vs); glAttachShader(engine->program, fs);
    glLinkProgram(engine->program);

    return 0;
}

static void engine_draw_frame(struct engine* engine) {
    if (engine->display == NULL) return;

    glClearColor(0.2f, 0.5f, 0.9f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    glUseProgram(engine->program);

    float proj[16], view[16], mvp[16];
    float aspect = (float)engine->width / (float)engine->height;
    mat4_perspective(proj, 1.0f, aspect, 0.1f, 100.0f);
    
    static float angle = 0; angle += 0.02f;
    mat4_translate(view, 0, 0, -5.0f);
    
    // Вращение (упрощенно через трансляцию для теста)
    view[12] = sinf(angle) * 2.0f; 

    mat4_mul(mvp, proj, view);
    GLint mLoc = glGetUniformLocation(engine->program, "m");
    glUniformMatrix4fv(mLoc, 1, GL_FALSE, mvp);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), cube_vertices);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), &cube_vertices[3]);
    glEnableVertexAttribArray(1);

    glViewport(0, 0, engine->width, engine->height);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    eglSwapBuffers(engine->display, engine->surface);
}

static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* engine = (struct engine*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (engine->app->window != NULL) {
                engine_init_display(engine);
                engine_draw_frame(engine);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            engine->display = EGL_NO_DISPLAY; // Очистка при закрытии
            break;
    }
}

void android_main(struct android_app* state) {
    struct engine engine = {0};
    state->userData = &engine;
    state->onAppCmd = engine_handle_cmd;
    engine.app = state;

    while (1) {
        int ident, events;
        struct android_poll_source* source;
        while ((ident = ALooper_pollOnce(0, NULL, &events, (void**)&source)) >= 0) {
            if (source != NULL) source->process(state, source);
            if (state->destroyRequested != 0) return;
        }
        if (engine.display != NULL) engine_draw_frame(&engine);
    }
}
