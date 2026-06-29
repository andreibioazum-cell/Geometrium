#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdbool.h>
#include <math.h>
#include "cube.h"
#include "math_utils.h"
#include "world.h"

// Определения массивов (один раз!)
unsigned char map[WORLD_SIZE][CHUNK_H][WORLD_SIZE];
unsigned char face_visible[WORLD_SIZE][CHUNK_H][WORLD_SIZE];

#define JOY_RADIUS      70.0f
#define JOY_Y_OFFSET    160.0f
#define JOY_X_OFFSET    140.0f
#define STICK_RADIUS    28.0f
#define JUMP_BTN_SIZE   55.0f
#define JUMP_BTN_OFFSET 130.0f
#define PI              3.14159265f

// Хитбокс игрока: 0.8 ширина, 2.0 высота
#define PLAYER_W        0.4f   // половина ширины
#define PLAYER_H        2.0f   // высота (от ног до макушки)
#define EYE_H           1.65f  // высота глаз от ног
#define HEAD_MARGIN     0.15f  // зазор до макушки от глаз

// FOV: было 1.57 (90°), минус 5% = ~85.5°
#define GAME_FOV        1.4915f

struct engine {
    struct android_app* app;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width, height;
    GLuint program;

    float camPos[3];  // позиция глаз
    float camRot[2];  // pitch, yaw
    float velY;
    float joyX, joyY;
    float moveDirX, moveDirZ;
    float lastTouchX, lastTouchY;
    bool isMoving;
    int movePointerId;
    int lookPointerId;
    bool onGround;

    // Кешированные данные для рендера
    GLuint vbo;
    int visibleFaceCount;
};

// ============= Коллизии =============

// Проверка блока по мировым координатам
static bool is_solid(float x, float y, float z) {
    int ix = (int)floorf(x + 0.5f);
    int iy = (int)floorf(y + 0.5f);
    int iz = (int)floorf(-z + 0.5f);
    if (ix < 0 || ix >= WORLD_SIZE ||
        iy < 0 || iy >= CHUNK_H    ||
        iz < 0 || iz >= WORLD_SIZE)
        return false;
    return map[ix][iy][iz] > 0;
}

// Проверяет все 4 угла хитбокса на заданной высоте
static bool check_collision_at(float x, float y, float z) {
    return is_solid(x - PLAYER_W, y, z - PLAYER_W) ||
           is_solid(x + PLAYER_W, y, z - PLAYER_W) ||
           is_solid(x - PLAYER_W, y, z + PLAYER_W) ||
           is_solid(x + PLAYER_W, y, z + PLAYER_W);
}

// Земля — проверяем чуть ниже ног
static bool check_ground(float x, float footY, float z) {
    return check_collision_at(x, footY - 0.05f, z);
}

// Потолок — проверяем макушку
static bool check_ceiling(float x, float headY, float z) {
    return check_collision_at(x, headY, z);
}

// Стены — проверяем на нескольких высотах тела
static bool check_wall(float x, float eyeY, float z) {
    float footY = eyeY - EYE_H;
    // Проверяем ноги, колени, пояс, грудь
    return check_collision_at(x, footY + 0.1f, z) ||
           check_collision_at(x, footY + 0.6f, z) ||
           check_collision_at(x, footY + 1.2f, z) ||
           check_collision_at(x, footY + 1.8f, z);
}

// Проверка что камера не внутри блока (антипроникновение)
static bool check_head_inside(float x, float eyeY, float z) {
    return is_solid(x, eyeY, z) ||
           is_solid(x - PLAYER_W, eyeY, z) ||
           is_solid(x + PLAYER_W, eyeY, z) ||
           is_solid(x, eyeY, z - PLAYER_W) ||
           is_solid(x, eyeY, z + PLAYER_W);
}

// ============= Физика =============
static void apply_physics(struct engine* eng) {
    // Гравитация
    eng->velY -= 0.012f;
    if (eng->velY < -0.5f) eng->velY = -0.5f;

    float eyeY = eng->camPos[1];
    float footY = eyeY - EYE_H;
    float headY = eyeY + HEAD_MARGIN;

    float nextEyeY = eyeY + eng->velY;
    float nextFootY = nextEyeY - EYE_H;
    float nextHeadY = nextEyeY + HEAD_MARGIN;

    eng->onGround = false;

    // Падение — проверяем ноги
    if (eng->velY <= 0) {
        if (check_ground(eng->camPos[0], nextFootY, eng->camPos[2])) {
            eng->velY = 0;
            eng->onGround = true;
            // Не двигаем вниз
        } else {
            eng->camPos[1] = nextEyeY;
        }
    }
    // Прыжок — проверяем потолок
    else {
        if (check_ceiling(eng->camPos[0], nextHeadY, eng->camPos[2])) {
            eng->velY = 0;
            // Остаёмся на месте
        } else {
            eng->camPos[1] = nextEyeY;
        }
    }

    // Движение по горизонтали
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

    // Антипроникновение камеры в стены
    if (check_head_inside(eng->camPos[0], eng->camPos[1], eng->camPos[2])) {
        // Выталкиваем вверх
        for (int i = 0; i < 10; i++) {
            eng->camPos[1] += 0.1f;
            if (!check_head_inside(eng->camPos[0], eng->camPos[1], eng->camPos[2]))
                break;
        }
    }

    // Респавн при падении в бездну
    if (eng->camPos[1] < -10.0f) {
        eng->camPos[0] = WORLD_SIZE / 2.0f;
        eng->camPos[2] = -(WORLD_SIZE / 2.0f);
        eng->camPos[1] = get_spawn_y((int)eng->camPos[0],
                                      (int)(-eng->camPos[2]));
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

        float jbX = eng->width - JUMP_BTN_OFFSET;
        float jbY = eng->height - JUMP_BTN_OFFSET;

        if (code == AMOTION_EVENT_ACTION_DOWN ||
            code == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            // Кнопка прыжка
            float djx = x - jbX, djy = y - jbY;
            if (sqrtf(djx*djx + djy*djy) < JUMP_BTN_SIZE * 1.5f &&
                eng->onGround) {
                eng->velY = 0.22f;
                continue;
            }

            if (x < eng->width / 2) {
                // Левая часть — джойстик
                eng->joyX = JOY_X_OFFSET;
                eng->joyY = eng->height - JOY_Y_OFFSET;
                eng->isMoving = true;
                eng->movePointerId = id;
                eng->moveDirX = 0;
                eng->moveDirZ = 0;
            } else {
                // Правая часть — камера
                eng->lastTouchX = x;
                eng->lastTouchY = y;
                eng->lookPointerId = id;
            }
        }
    }

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
                    eng->moveDirX = 0;
                    eng->moveDirZ = 0;
                }
            }
            if (id == eng->lookPointerId) {
                eng->camRot[1] += (x - eng->lastTouchX) * 0.004f;
                eng->camRot[0] += (y - eng->lastTouchY) * 0.004f;
                if (eng->camRot[0] > 1.4f) eng->camRot[0] = 1.4f;
                if (eng->camRot[0] < -1.4f) eng->camRot[0] = -1.4f;
                eng->lastTouchX = x;
                eng->lastTouchY = y;
            }
        }
    }

    if (code == AMOTION_EVENT_ACTION_UP ||
        code == AMOTION_EVENT_ACTION_POINTER_UP) {
        int pi = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                  AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int id = AMotionEvent_getPointerId(event, pi);
        if (id == eng->movePointerId) {
            eng->isMoving = false;
            eng->moveDirX = 0;
            eng->moveDirZ = 0;
        }
        if (id == eng->lookPointerId)
            eng->lookPointerId = -1;
    }
    return 1;
}

// ============= UI =============
static GLuint uiProg = 0;

static void init_ui_shader(void) {
    const char* vS =
        "attribute vec4 p; void main(){ gl_Position=p; }";
    const char* fS =
        "precision mediump float; uniform vec4 col;"
        "void main(){ gl_FragColor=col; }";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vS, NULL);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fS, NULL);
    glCompileShader(fs);
    uiProg = glCreateProgram();
    glAttachShader(uiProg, vs);
    glAttachShader(uiProg, fs);
    glLinkProgram(uiProg);
    glDeleteShader(vs);
    glDeleteShader(fs);
}

static void draw_ring(float cx, float cy, float r, float thickness,
                      int w, int h,
                      float cr, float cg, float cb, float ca) {
    float ndcX = (cx / w) * 2.0f - 1.0f;
    float ndcY = 1.0f - (cy / h) * 2.0f;
    float rx_out = (r / w) * 2.0f;
    float ry_out = (r / h) * 2.0f;
    float rx_in = ((r - thickness) / w) * 2.0f;
    float ry_in = ((r - thickness) / h) * 2.0f;

    int segs = 32;
    float verts[(32 + 1) * 4]; // замкнутое кольцо
    for (int i = 0; i <= segs; i++) {
        float a = (float)i / segs * 2.0f * PI;
        float ca2 = cosf(a), sa = sinf(a);
        verts[i*4+0] = ndcX + ca2 * rx_out;
        verts[i*4+1] = ndcY + sa  * ry_out;
        verts[i*4+2] = ndcX + ca2 * rx_in;
        verts[i*4+3] = ndcY + sa  * ry_in;
    }
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg, "col"), cr, cg, cb, ca);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, (segs + 1) * 2);
}

static void draw_circle_filled(float cx, float cy, float r,
                                int w, int h,
                                float cr, float cg, float cb, float ca) {
    float ndcX = (cx / w) * 2.0f - 1.0f;
    float ndcY = 1.0f - (cy / h) * 2.0f;
    float rx = (r / w) * 2.0f;
    float ry = (r / h) * 2.0f;

    int segs = 24; // Уменьшено для экономии
    float verts[(24 + 2) * 2];
    verts[0] = ndcX;
    verts[1] = ndcY;
    for (int i = 0; i <= segs; i++) {
        float a = (float)i / segs * 2.0f * PI;
        verts[(i+1)*2+0] = ndcX + cosf(a) * rx;
        verts[(i+1)*2+1] = ndcY + sinf(a) * ry;
    }
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg, "col"), cr, cg, cb, ca);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, segs + 2);
}

static void draw_ui(struct engine* eng) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float jx = JOY_X_OFFSET;
    float jy = eng->height - JOY_Y_OFFSET;

    // Обводка джойстика
    draw_ring(jx, jy, JOY_RADIUS, 3.0f,
              eng->width, eng->height, 0, 0, 0, 0.8f);

    // Стик
    float hx = jx + eng->moveDirX * JOY_RADIUS * 0.7f;
    float hy = jy + eng->moveDirZ * JOY_RADIUS * 0.7f;
    draw_circle_filled(hx, hy, STICK_RADIUS,
                       eng->width, eng->height, 0, 0, 0, 0.7f);

    // Кнопка прыжка
    float bx = eng->width - JUMP_BTN_OFFSET;
    float by = eng->height - JUMP_BTN_OFFSET;
    draw_ring(bx, by, JUMP_BTN_SIZE, 3.0f,
              eng->width, eng->height, 0, 0, 0, 0.8f);

    // Стрелка вверх
    float arrowSize = JUMP_BTN_SIZE * 0.4f;
    float ndcBx = (bx / eng->width) * 2.0f - 1.0f;
    float ndcBy = 1.0f - (by / eng->height) * 2.0f;
    float arx = (arrowSize / eng->width) * 2.0f;
    float ary = (arrowSize / eng->height) * 2.0f;
    float arrow[] = {
        ndcBx,       ndcBy + ary,
        ndcBx - arx, ndcBy - ary * 0.5f,
        ndcBx + arx, ndcBy - ary * 0.5f
    };
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg, "col"), 0, 0, 0, 0.7f);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, arrow);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

// ============= VBO для мира =============

// Строим VBO только из видимых граней — огромная экономия памяти и GPU
static void rebuild_world_vbo(struct engine* eng) {
    // Считаем видимые грани
    int faceCount = 0;
    for (int x = 0; x < WORLD_SIZE; x++)
        for (int y = 0; y < CHUNK_H; y++)
            for (int z = 0; z < WORLD_SIZE; z++) {
                unsigned char f = face_visible[x][y][z];
                if (f & FACE_PX) faceCount++;
                if (f & FACE_NX) faceCount++;
                if (f & FACE_PY) faceCount++;
                if (f & FACE_NY) faceCount++;
                if (f & FACE_PZ) faceCount++;
                if (f & FACE_NZ) faceCount++;
            }

    eng->visibleFaceCount = faceCount;
    if (faceCount == 0) return;

    // 6 вершин на грань, 5 float на вершину
    int vertCount = faceCount * 6;
    float* buf = (float*)malloc(vertCount * 5 * sizeof(float));
    int idx = 0;

    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int y = 0; y < CHUNK_H; y++) {
            for (int z = 0; z < WORLD_SIZE; z++) {
                unsigned char f = face_visible[x][y][z];
                if (!f) continue;

                float bx = (float)x;
                float by = (float)y;
                float bz = (float)(-z);

                // Каждая грань = 6 вершин (2 треугольника)
                // front (z+) = bz + 0.5
                if (f & FACE_PZ) {
                    float v[] = {
                        bx-0.5f, by-0.5f, bz+0.5f, 0,0,
                        bx+0.5f, by-0.5f, bz+0.5f, 1,0,
                        bx+0.5f, by+0.5f, bz+0.5f, 1,1,
                        bx-0.5f, by-0.5f, bz+0.5f, 0,0,
                        bx+0.5f, by+0.5f, bz+0.5f, 1,1,
                        bx-0.5f, by+0.5f, bz+0.5f, 0,1
                    };
                    memcpy(&buf[idx], v, sizeof(v));
                    idx += 30;
                }
                // back (z-) = bz - 0.5
                if (f & FACE_NZ) {
                    float v[] = {
                        bx-0.5f, by-0.5f, bz-0.5f, 0,0,
                        bx-0.5f, by+0.5f, bz-0.5f, 0,1,
                        bx+0.5f, by+0.5f, bz-0.5f, 1,1,
                        bx-0.5f, by-0.5f, bz-0.5f, 0,0,
                        bx+0.5f, by+0.5f, bz-0.5f, 1,1,
                        bx+0.5f, by-0.5f, bz-0.5f, 1,0
                    };
                    memcpy(&buf[idx], v, sizeof(v));
                    idx += 30;
                }
                // right (x+)
                if (f & FACE_PX) {
                    float v[] = {
                        bx+0.5f, by-0.5f, bz-0.5f, 0,0,
                        bx+0.5f, by+0.5f, bz-0.5f, 0,1,
                        bx+0.5f, by+0.5f, bz+0.5f, 1,1,
                        bx+0.5f, by-0.5f, bz-0.5f, 0,0,
                        bx+0.5f, by+0.5f, bz+0.5f, 1,1,
                        bx+0.5f, by-0.5f, bz+0.5f, 1,0
                    };
                    memcpy(&buf[idx], v, sizeof(v));
                    idx += 30;
                }
                // left (x-)
                if (f & FACE_NX) {
                    float v[] = {
                        bx-0.5f, by-0.5f, bz-0.5f, 0,0,
                        bx-0.5f, by-0.5f, bz+0.5f, 1,0,
                        bx-0.5f, by+0.5f, bz+0.5f, 1,1,
                        bx-0.5f, by-0.5f, bz-0.5f, 0,0,
                        bx-0.5f, by+0.5f, bz+0.5f, 1,1,
                        bx-0.5f, by+0.5f, bz-0.5f, 0,1
                    };
                    memcpy(&buf[idx], v, sizeof(v));
                    idx += 30;
                }
                // top (y+)
                if (f & FACE_PY) {
                    float v[] = {
                        bx-0.5f, by+0.5f, bz-0.5f, 0,0,
                        bx-0.5f, by+0.5f, bz+0.5f, 1,0,
                        bx+0.5f, by+0.5f, bz+0.5f, 1,1,
                        bx-0.5f, by+0.5f, bz-0.5f, 0,0,
                        bx+0.5f, by+0.5f, bz+0.5f, 1,1,
                        bx+0.5f, by+0.5f, bz-0.5f, 0,1
                    };
                    memcpy(&buf[idx], v, sizeof(v));
                    idx += 30;
                }
                // bottom (y-)
                if (f & FACE_NY) {
                    float v[] = {
                        bx-0.5f, by-0.5f, bz-0.5f, 0,0,
                        bx+0.5f, by-0.5f, bz-0.5f, 1,0,
                        bx+0.5f, by-0.5f, bz+0.5f, 1,1,
                        bx-0.5f, by-0.5f, bz-0.5f, 0,0,
                        bx+0.5f, by-0.5f, bz+0.5f, 1,1,
                        bx-0.5f, by-0.5f, bz+0.5f, 0,1
                    };
                    memcpy(&buf[idx], v, sizeof(v));
                    idx += 30;
                }
            }
        }
    }

    if (!eng->vbo) glGenBuffers(1, &eng->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 vertCount * 5 * sizeof(float),
                 buf, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    free(buf);
}

// ============= Рендер =============
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

    float proj[16], view[16], mvp[16];
    mat4_perspective(proj, GAME_FOV,
                     (float)eng->width / eng->height,
                     0.1f, 80.0f);
    mat4_lookat(view, eng->camPos, eng->camRot[0], eng->camRot[1]);
    mat4_mul(mvp, proj, view);

    GLint mLoc = glGetUniformLocation(eng->program, "m");
    glUniformMatrix4fv(mLoc, 1, GL_FALSE, mvp);

    // Один draw call для всего мира!
    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * 4, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * 4, (void*)12);
    glEnableVertexAttribArray(1);
    glDrawArrays(GL_TRIANGLES, 0, eng->visibleFaceCount * 6);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    draw_ui(eng);
    eglSwapBuffers(eng->display, eng->surface);
}

// ============= Инициализация =============
static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) {
        eng->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(eng->display, 0, 0);
        EGLConfig config;
        EGLint n;
        EGLint att[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_DEPTH_SIZE, 16,
            EGL_NONE
        };
        eglChooseConfig(eng->display, att, &config, 1, &n);
        eng->surface = eglCreateWindowSurface(eng->display, config,
                                               eng->app->window, NULL);
        EGLint ctxAtt[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        eng->context = eglCreateContext(eng->display, config,
                                         NULL, ctxAtt);
        eglMakeCurrent(eng->display, eng->surface,
                       eng->surface, eng->context);

        // Шейдер мира — модель уже в вершинах, MVP без model
        const char* vS =
            "attribute vec4 p;"
            "attribute vec2 t;"
            "varying vec2 vT;"
            "uniform mat4 m;"
            "void main(){ gl_Position=m*p; vT=t; }";
        const char* fS =
            "precision mediump float;"
            "varying vec2 vT;"
            "void main(){"
            "  gl_FragColor=vec4(vT.x, vT.y+0.3, 0.4, 1.0);"
            "}";
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vS, NULL);
        glCompileShader(vs);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fS, NULL);
        glCompileShader(fs);
        eng->program = glCreateProgram();
        glAttachShader(eng->program, vs);
        glAttachShader(eng->program, fs);
        glLinkProgram(eng->program);
        glDeleteShader(vs);
        glDeleteShader(fs);

        init_ui_shader();

        // Строим VBO один раз
        rebuild_world_vbo(eng);
    }
}

void android_main(struct android_app* state) {
    struct engine eng = {0};
    eng.movePointerId = -1;
    eng.lookPointerId = -1;
    generate_world();

    eng.camPos[0] = WORLD_SIZE / 2.0f;
    eng.camPos[2] = -(WORLD_SIZE / 2.0f);
    eng.camPos[1] = get_spawn_y((int)eng.camPos[0],
                                 (int)(-eng.camPos[2]));

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
