#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "cube.h"
#include "math_utils.h"

struct engine {
    struct android_app* app;
    EGLDisplay display; EGLContext context; EGLSurface surface;
    int32_t width, height;
    GLuint program;
};

static void engine_init_display(struct engine* engine) {
    engine->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(engine->display, 0, 0);
    EGLConfig config; EGLint n;
    EGLint att[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_DEPTH_SIZE, 16, EGL_NONE };
    eglChooseConfig(engine->display, att, &config, 1, &n);
    engine->surface = eglCreateWindowSurface(engine->display, config, engine->app->window, NULL);
    engine->context = eglCreateContext(engine->display, config, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
    eglMakeCurrent(engine->display, engine->surface, engine->surface, engine->context);
}

static void engine_draw_frame(struct engine* engine) {
    if (!engine->display) return;
    
    // Каждый кадр проверяем размер, чтобы не было сплющивания
    eglQuerySurface(engine->display, engine->surface, EGL_WIDTH, &engine->width);
    eglQuerySurface(engine->display, engine->surface, EGL_HEIGHT, &engine->height);
    glViewport(0, 0, engine->width, engine->height);

    glClearColor(0.1f, 0.5f, 0.9f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    
    glUseProgram(engine->program);
    GLint mLoc = glGetUniformLocation(engine->program, "m");

    float proj[16], view[16], rotX[16], rotY[16], mvp[16], t1[16], t2[16];
    
    // FOV 60 градусов, правильный Aspect Ratio
    float aspect = (float)engine->width / (float)engine->height;
    mat4_perspective(proj, 1.04f, aspect, 0.1f, 100.0f);
    
    mat4_identity(view);
    view[14] = -4.0f; // Отодвигаем камеру

    static float a = 0; a += 0.02f;
    mat4_rotate_x(rotX, a * 0.3f); // Медленно по X
    mat4_rotate_y(rotY, a);        // Быстрее по Y
    
    // MVP = proj * view * rotX * rotY
    mat4_mul(t1, proj, view);
    mat4_mul(t2, t1, rotX);
    mat4_mul(mvp, t2, rotY);

    glUniformMatrix4fv(mLoc, 1, GL_FALSE, mvp);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*4, cube_vertices);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*4, &cube_vertices[3]);
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLES, 0, 36);
    eglSwapBuffers(engine->display, engine->surface);
}

// (Остальной код engine_handle_cmd и android_main остается таким же как в прошлый раз)
static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* engine = (struct engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) {
        engine_init_display(engine);
        const char* vS = "attribute vec4 p; attribute vec2 t; varying vec2 vT; uniform mat4 m; void main(){ gl_Position=m*p; vT=t; }";
        const char* fS = "precision mediump float; varying vec2 vT; void main(){ gl_FragColor=vec4(vT, 0.7, 1.0); }";
        GLuint vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs, 1, &vS, NULL); glCompileShader(vs);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs, 1, &fS, NULL); glCompileShader(fs);
        engine->program = glCreateProgram();
        glAttachShader(engine->program, vs); glAttachShader(engine->program, fs);
        glLinkProgram(engine->program);
    }
}

void android_main(struct android_app* state) {
    struct engine engine = {0};
    state->userData = &engine;
    state->onAppCmd = engine_handle_cmd;
    engine.app = state;
    while (1) {
        int id, ev; struct android_poll_source* s;
        while ((id = ALooper_pollOnce(0, NULL, &ev, (void**)&s)) >= 0) {
            if (s) s->process(state, s);
            if (state->destroyRequested) return;
        }
        engine_draw_frame(&engine);
    }
}
