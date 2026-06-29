#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdbool.h>
#include <math.h>
#include "cube.h"
#include "math_utils.h"
#include "world.h"

#define JOY_RADIUS 120.0f
#define JOY_Y_OFFSET 200.0f
#define JOY_X_OFFSET 180.0f
#define JUMP_BTN_SIZE 100.0f
#define JUMP_BTN_OFFSET 180.0f

struct engine {
    struct android_app* app;
    EGLDisplay display; EGLSurface surface; EGLContext context;
    int32_t width, height;
    GLuint program;
    GLuint uiProgram;

    float camPos[3], camRot[2];
    float velY;
    float joyX, joyY;
    float moveDirX, moveDirZ;
    float lastTouchX, lastTouchY;
    bool isMoving;
    int movePointerId;
    int lookPointerId;
    bool onGround;
};

// ============= Коллизии =============
bool is_solid(float x, float y, float z) {
    int ix = (int)floorf(x + 0.5f);
    int iy = (int)floorf(y + 0.5f);
    int iz = (int)floorf(-z + 0.5f);
    if (ix < 0 || ix >= WORLD_SIZE || iz < 0 || iz >= WORLD_SIZE || iy < 0 || iy >= CHUNK_H)
        return false;
    return map[ix][iy][iz] > 0;
}

// Проверка земли под ногами (4 угла вокруг игрока)
bool check_ground(float x, float y, float z) {
    float w = 0.25f;
    return is_solid(x-w, y, z-w) || is_solid(x+w, y, z-w) ||
           is_solid(x-w, y, z+w) || is_solid(x+w, y, z+w);
}

// Проверка стен (2 точки по высоте)
bool check_wall(float x, float y, float z) {
    return is_solid(x, y - 0.5f, z) || is_solid(x, y - 1.0f, z);
}

// ============= Физика =============
static void apply_physics(struct engine* eng) {
    eng->velY -= 0.012f;
    if (eng->velY < -0.5f) eng->velY = -0.5f;
    float nextY = eng->camPos[1] + eng->velY;

    // Пол
    eng->onGround = false;
    if (check_ground(eng->camPos[0], nextY - 1.7f, eng->camPos[2])) {
        eng->velY = 0;
        eng->onGround = true;
    } else {
        eng->camPos[1] = nextY;
    }

    // Потолок
    if (eng->velY > 0 && check_ground(eng->camPos[0], nextY + 0.2f, eng->camPos[2])) {
        eng->velY = 0;
    }

    // Движение
    if (eng->isMoving && (eng->moveDirX != 0 || eng->moveDirZ != 0)) {
        float speed = 0.08f;
        float yaw = eng->camRot[1];
        float fX = sinf(yaw), fZ = -cosf(yaw);
        float rX = cosf(yaw), rZ = sinf(yaw);
        float dx = (fX * -eng->moveDirZ + rX * eng->moveDirX) * speed;
        float dz = (fZ * -eng->moveDirZ + rZ * eng->moveDirX) * speed;

        // Раздельная проверка по осям
        if (!check_wall(eng->camPos[0] + dx, eng->camPos[1], eng->camPos[2]))
            eng->camPos[0] += dx;
        if (!check_wall(eng->camPos[0], eng->camPos[1], eng->camPos[2] + dz))
            eng->camPos[2] += dz;
    }

    if (eng->camPos[1] < -5.0f) {
        eng->camPos[0] = WORLD_SIZE / 2;
        eng->camPos[2] = -(WORLD_SIZE / 2);
        eng->camPos[1] = get_spawn_y((int)eng->camPos[0], (int)-eng->camPos[2]);
        eng->velY = 0;
    }
}

// ============= Управление =============
static int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;

    int action = AMotionEvent_getAction(event);
    int code = action & AMOTION_EVENT_ACTION_MASK;
    int pCount = AMotionEvent_getPointerCount(event);

    for (int i = 0; i < pCount; i++) {
        float x = AMotionEvent_getX(event, i);
        float y = AMotionEvent_getY(event, i);
        int id = AMotionEvent_getPointerId(event, i);

        // Кнопка прыжка (справа внизу)
        float jbX = eng->width - JUMP_BTN_OFFSET;
        float jbY = eng->height - JUMP_BTN_OFFSET;
        if (code == AMOTION_EVENT_ACTION_DOWN || code == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            float djx = x - jbX, djy = y - jbY;
            if (sqrtf(djx*djx + djy*djy) < JUMP_BTN_SIZE && eng->onGround) {
                eng->velY = 0.22f;
                continue;
            }
        }

        if (x < eng->width / 2) { // Левая часть - Джойстик
            if (code == AMOTION_EVENT_ACTION_DOWN || code == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                eng->joyX = JOY_X_OFFSET;
                eng->joyY = eng->height - JOY_Y_OFFSET;
                eng->isMoving = true;
                eng->movePointerId = id;
                eng->moveDirX = 0; eng->moveDirZ = 0;
            }
        } else { // Правая часть - Обзор
            if (code == AMOTION_EVENT_ACTION_DOWN || code == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                eng->lastTouchX = x; eng->lastTouchY = y;
                eng->lookPointerId = id;
            }
        }
    }

    // Обработка движения пальцев
    if (code == AMOTION_EVENT_ACTION_MOVE) {
        for (int i = 0; i < pCount; i++) {
            float x = AMotionEvent_getX(event, i);
            float y = AMotionEvent_getY(event, i);
            int id = AMotionEvent_getPointerId(event, i);

            if (id == eng->movePointerId && eng->isMoving) {
                float dx = x - eng->joyX, dy = y - eng->joyY;
                float d = sqrtf(dx*dx + dy*dy);
                if (d > 10.0f) {
                    float clamp = d > JOY_RADIUS ? JOY_RADIUS : d;
                    eng->moveDirX = (dx / d) * (clamp / JOY_RADIUS);
                    eng->moveDirZ = (dy / d) * (clamp / JOY_RADIUS);
                } else {
                    eng->moveDirX = 0; eng->moveDirZ = 0;
                }
            }
            if (id == eng->lookPointerId) {
                eng->camRot[1] += (x - eng->lastTouchX) * 0.004f;
                eng->camRot[0] += (y - eng->lastTouchY) * 0.004f;

                // ОГРАНИЧЕНИЕ КАМЕРЫ: не дать перевернуться
                if (eng->camRot[0] > 1.4f) eng->camRot[0] = 1.4f;
                if (eng->camRot[0] < -1.4f) eng->camRot[0] = -1.4f;

                eng->lastTouchX = x; eng->lastTouchY = y;
            }
        }
    }

    if (code == AMOTION_EVENT_ACTION_UP || code == AMOTION_EVENT_ACTION_POINTER_UP) {
        int pi = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int id = AMotionEvent_getPointerId(event, pi);
        if (id == eng->movePointerId) {
            eng->isMoving = false; eng->moveDirX = 0; eng->moveDirZ = 0;
        }
        if (id == eng->lookPointerId) eng->lookPointerId = -1;
    }
    return 1;
}

// ============= UI Рендер =============
static GLuint uiProg = 0;

static void init_ui_shader() {
    const char* vS = "attribute vec4 p; void main(){ gl_Position=p; }";
    const char* fS = "precision mediump float; uniform vec4 col; void main(){ gl_FragColor=col; }";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs, 1, &vS, NULL); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs, 1, &fS, NULL); glCompileShader(fs);
    uiProg = glCreateProgram();
    glAttachShader(uiProg, vs); glAttachShader(uiProg, fs);
    glLinkProgram(uiProg);
}

static void draw_circle(float cx, float cy, float r, int w, int h, float cr, float cg, float cb, float ca) {
    float ndcX = (cx / w) * 2.0f - 1.0f;
    float ndcY = 1.0f - (cy / h) * 2.0f;
    float rx = (r / w) * 2.0f;
    float ry = (r / h) * 2.0f;

    float verts[64 * 2];
    int segs = 32;
    for (int i = 0; i < segs; i++) {
        float a = (float)i / segs * 6.283f;
        verts[i*2+0] = ndcX + cosf(a) * rx;
        verts[i*2+1] = ndcY + sinf(a) * ry;
    }
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg, "col"), cr, cg, cb, ca);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, segs);
}

static void draw_ui(struct engine* eng) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float jx = JOY_X_OFFSET, jy = eng->height - JOY_Y_OFFSET;

    // Круг джойстика (внешний)
    draw_circle(jx, jy, JOY_RADIUS, eng->width, eng->height, 1, 1, 1, 0.2f);

    // Ручка джойстика (внутренний)
    float hx = jx + eng->moveDirX * JOY_RADIUS * 0.7f;
    float hy = jy + eng->moveDirZ * JOY_RADIUS * 0.7f;
    draw_circle(hx, hy, 40, eng->width, eng->height, 1, 1, 1, 0.5f);

    // Кнопка прыжка
    float bx = eng->width - JUMP_BTN_OFFSET;
    float by = eng->height - JUMP_BTN_OFFSET;
    draw_circle(bx, by, JUMP_BTN_SIZE, eng->width, eng->height, 0.3f, 0.9f, 0.3f, 0.4f);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

// ============= Рендер мира =============
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
    for (int x=0; x<WORLD_SIZE; x++) {
        for (int y=0; y<CHUNK_H; y++) {
            for (int z=0; z<WORLD_SIZE; z++) {
                if (map[x][y][z]) {
                    mat4_translate(model, (float)x, (float)y, (float)-z);
                    mat4_mul(tmp, proj, view);
                    mat4_mul(mvp, tmp, model);
                    glUniformMatrix4fv(mLoc, 1, GL_FALSE, mvp);
                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }
            }
        }
    }

    // UI поверх мира
    draw_ui(eng);

    eglSwapBuffers(eng->display, eng->surface);
}

// ============= Инициализация =============
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

        init_ui_shader();
    }
}

void android_main(struct android_app* state) {
    struct engine eng = {0};
    eng.movePointerId = -1;
    eng.lookPointerId = -1;
    generate_world();

    eng.camPos[0] = WORLD_SIZE / 2;
    eng.camPos[2] = -(WORLD_SIZE / 2);
    eng.camPos[1] = get_spawn_y((int)eng.camPos[0], (int)-eng.camPos[2]);

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
