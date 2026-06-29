#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdbool.h>
#include <math.h>
#include "cube.h"
#include "math_utils.h"
#include "world.h"

struct engine {
    struct android_app* app;
    EGLDisplay display; EGLSurface surface; EGLContext context;
    int32_t width, height;
    GLuint program;

    // Физика и позиция
    float camPos[3], camRot[2];
    float velY;
    
    // Управление (Джойстик)
    float joyStartX, joyStartY; // Центр джойстика
    float moveDirX, moveDirZ;   // Вектор движения
    bool isMoving;
    
    // Обзор
    float lastTouchX, lastTouchY;
    bool isLooking;
};

bool is_solid(float x, float y, float z) {
    int ix = (int)roundf(x);
    int iy = (int)floorf(y);
    int iz = (int)roundf(-z);
    if (ix < 0 || ix >= WORLD_SIZE || iz < 0 || iz >= WORLD_SIZE || iy < 0 || iy >= CHUNK_H) 
        return false;
    return map[ix][iy][iz] == 1;
}

static int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        int action = AMotionEvent_getAction(event);
        int pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        float x = AMotionEvent_getX(event, pointerIndex);
        float y = AMotionEvent_getY(event, pointerIndex);
        int actionCode = action & AMOTION_EVENT_ACTION_MASK;

        if (x < eng->width / 2) { // ЛЕВАЯ ЧАСТЬ (Ходьба)
            if (actionCode == AMOTION_EVENT_ACTION_DOWN || actionCode == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                eng->joyStartX = x; eng->joyStartY = y;
                eng->isMoving = true;
            } else if (actionCode == AMOTION_EVENT_ACTION_MOVE) {
                float dx = x - eng->joyStartX;
                float dy = y - eng->joyStartY;
                float dist = sqrtf(dx*dx + dy*dy);
                if (dist > 10.0f) { // Мертвая зона
                    eng->moveDirX = dx / dist;
                    eng->moveDirZ = dy / dist;
                }
            } else if (actionCode == AMOTION_EVENT_ACTION_UP || actionCode == AMOTION_EVENT_ACTION_POINTER_UP) {
                eng->isMoving = false;
                eng->moveDirX = 0; eng->moveDirZ = 0;
            }
        } else { // ПРАВАЯ ЧАСТЬ (Обзор и Прыжок)
            if (actionCode == AMOTION_EVENT_ACTION_DOWN || actionCode == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                eng->lastTouchX = x; eng->lastTouchY = y;
                eng->isLooking = true;
                // Прыжок если стоим на земле
                if (eng->velY == 0) eng->velY = 0.25f; 
            } else if (actionCode == AMOTION_EVENT_ACTION_MOVE) {
                eng->camRot[1] += (x - eng->lastTouchX) * 0.005f;
                eng->camRot[0] += (y - eng->lastTouchY) * 0.005f;
                eng->lastTouchX = x; eng->lastTouchY = y;
            } else if (actionCode == AMOTION_EVENT_ACTION_UP || actionCode == AMOTION_EVENT_ACTION_POINTER_UP) {
                eng->isLooking = false;
            }
        }
        return 1;
    }
    return 0;
}

static void apply_physics(struct engine* eng) {
    // 1. Гравитация
    eng->velY -= 0.015f;
    float nextY = eng->camPos[1] + eng->velY;

    if (is_solid(eng->camPos[0], nextY - 1.6f, eng->camPos[2])) {
        eng->velY = 0;
    } else {
        eng->camPos[1] = nextY;
    }

    // 2. Движение в любом направлении (360 градусов)
    if (eng->isMoving) {
        float speed = 0.12f;
        // Направление взгляда
        float yaw = eng->camRot[1];
        // Вектор вперед/назад
        float forwardX = sinf(yaw);
        float forwardZ = -cosf(yaw);
        // Вектор влево/вправо (перпендикуляр)
        float rightX = cosf(yaw);
        float rightZ = sinf(yaw);

        // Смешиваем векторы в зависимости от наклона джойстика
        float dirX = (forwardX * -eng->moveDirZ + rightX * eng->moveDirX) * speed;
        float dirZ = (forwardZ * -eng->moveDirZ + rightZ * eng->moveDirX) * speed;

        if (!is_solid(eng->camPos[0] + dirX, eng->camPos[1] - 0.5f, eng->camPos[2] + dirZ)) {
            eng->camPos[0] += dirX;
            eng->camPos[2] += dirZ;
        }
    }

    // Респавн если упал
    if (eng->camPos[1] < -5.0f) {
        eng->camPos[0] = 5; eng->camPos[1] = 8; eng->camPos[2] = 5;
    }
}

// Рендеринг (без изменений, но с вызовом физики)
static void engine_draw_frame(struct engine* eng) {
    if (!eng->display) return;
    apply_physics(eng);

    eglQuerySurface(eng->display, eng->surface, EGL_WIDTH, &eng->width);
    eglQuerySurface(eng->display, eng->surface, EGL_HEIGHT, &eng->height);
    glViewport(0, 0, eng->width, eng->height);
    glClearColor(0.5f, 0.8f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(eng->program);
    
    float proj[16], view[16], model[16], mvp[16], tmp[16];
    mat4_perspective(proj, 1.0f, (float)eng->width/eng->height, 0.1f, 100.0f);
    mat4_lookat(view, eng->camPos, eng->camRot[0], eng->camRot[1]);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*4, cube_vertices);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*4, &cube_vertices[3]);
    glEnableVertexAttribArray(1);

    GLint mLoc = glGetUniformLocation(eng->program, "m");
    for(int x=0; x<WORLD_SIZE; x++) {
        for(int y=0; y<CHUNK_H; y++) {
            for(int z=0; z<WORLD_SIZE; z++) {
                if(map[x][y][z]) {
                    mat4_translate(model, (float)x, (float)y, (float)-z);
                    mat4_mul(tmp, proj, view);
                    mat4_mul(mvp, tmp, model);
                    glUniformMatrix4fv(mLoc, 1, GL_FALSE, mvp);
                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }
            }
        }
    }
    eglSwapBuffers(eng->display, eng->surface);
}

// Стандартные функции Native App Glue
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
        
        const char* vS = "attribute vec4 p; attribute vec2 t; varying vec2 vT; uniform mat4 m; void main(){ gl_Position=m*p; vT=t; }";
        const char* fS = "precision mediump float; varying vec2 vT; void main(){ gl_FragColor=vec4(vT.x, vT.y + 0.3, 0.4, 1.0); }";
        GLuint vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs, 1, &vS, NULL); glCompileShader(vs);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs, 1, &fS, NULL); glCompileShader(fs);
        eng->program = glCreateProgram();
        glAttachShader(eng->program, vs); glAttachShader(eng->program, fs);
        glLinkProgram(eng->program);
    }
}

void android_main(struct android_app* state) {
    struct engine eng = {0};
    eng.camPos[0] = 5.0f; eng.camPos[1] = 8.0f; eng.camPos[2] = 5.0f;
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
