#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "engine.h"
#include "world.h"
#include "math_utils.h"
#include "physics.h"
#include "input.h"
#include "render.h"

static void engine_draw_frame(struct engine* eng) {
    if (!eng->display) return;

    update_world(eng);
    apply_physics(eng);

    eglQuerySurface(eng->display, eng->surface, EGL_WIDTH, &eng->width);
    eglQuerySurface(eng->display, eng->surface, EGL_HEIGHT, &eng->height);

    glViewport(0, 0, eng->width, eng->height);
    glClearColor(0.5f, 0.8f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    render_world(eng);
    draw_ui(eng);

    eglSwapBuffers(eng->display, eng->surface);
}

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
        eng->surface = eglCreateWindowSurface(
            eng->display, config, eng->app->window, NULL);
        EGLint ctxAtt[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        eng->context = eglCreateContext(
            eng->display, config, NULL, ctxAtt);
        eglMakeCurrent(eng->display, eng->surface,
                       eng->surface, eng->context);

        /*
         * Шейдер с освещением:
         * - Направленный свет сверху-сбоку (солнце)
         * - Ambient чтобы тени не были чёрными
         * - Каждая грань получает свою яркость по нормали
         */
        const char* vS =
            "attribute vec3 pos;"
            "attribute vec2 uv;"
            "attribute vec3 norm;"
            "varying vec2 vUV;"
            "varying vec3 vNorm;"
            "varying vec3 vWorldPos;"
            "uniform mat4 m;"
            "void main(){"
            "  gl_Position = m * vec4(pos, 1.0);"
            "  vUV = uv;"
            "  vNorm = norm;"
            "  vWorldPos = pos;"
            "}";

        const char* fS =
            "precision mediump float;"
            "varying vec2 vUV;"
            "varying vec3 vNorm;"
            "varying vec3 vWorldPos;"
            "uniform sampler2D tex;"
            "void main(){"
            "  vec4 texCol = texture2D(tex, vUV);"
            ""
            "  /* Солнце — сверху-справа-спереди */"
            "  vec3 lightDir = normalize(vec3(0.4, 0.9, 0.3));"
            ""
            "  /* Diffuse — основное освещение */"
            "  float diff = max(dot(vNorm, lightDir), 0.0);"
            ""
            "  /* Ambient — чтобы тени не были чёрными */"
            "  float ambient = 0.45;"
            ""
            "  /* Верхние грани ярче, нижние темнее */"
            "  float topBoost = 0.0;"
            "  if (vNorm.y > 0.5) topBoost = 0.1;"
            "  if (vNorm.y < -0.5) topBoost = -0.15;"
            ""
            "  /* Боковые грани чуть разные для объёмности */"
            "  float sideShade = 0.0;"
            "  if (abs(vNorm.x) > 0.5) sideShade = -0.05;"
            "  if (abs(vNorm.z) > 0.5) sideShade = -0.1;"
            ""
            "  float light = ambient + diff * 0.55 + topBoost + sideShade;"
            "  light = clamp(light, 0.2, 1.0);"
            ""
            "  /* Лёгкий туман на дальних расстояниях */"
            "  float dist = length(vWorldPos.xz);"
            "  float fog = clamp((dist - 30.0) / 50.0, 0.0, 0.6);"
            "  vec3 fogCol = vec3(0.5, 0.8, 1.0);"
            ""
            "  vec3 col = texCol.rgb * light;"
            "  col = mix(col, fogCol, fog);"
            ""
            "  gl_FragColor = vec4(col, texCol.a);"
            "}";

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vS, NULL);
        glCompileShader(vs);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fS, NULL);
        glCompileShader(fs);
        eng->program = glCreateProgram();

        /* Привязываем атрибуты до линковки */
        glBindAttribLocation(eng->program, 0, "pos");
        glBindAttribLocation(eng->program, 1, "uv");
        glBindAttribLocation(eng->program, 2, "norm");

        glAttachShader(eng->program, vs);
        glAttachShader(eng->program, fs);
        glLinkProgram(eng->program);
        glDeleteShader(vs);
        glDeleteShader(fs);

        eng->texture = load_texture(eng->app, "grass.png");

        init_ui_shader();
    }
}

void android_main(struct android_app* state) {
    struct engine eng = {0};
    eng.movePointerId = -1;
    eng.lookPointerId = -1;
    eng.worldLoaded = false;
    eng.meshDirty = true;

    eng.camPos[0] = 0.5f;
    eng.camPos[2] = -0.5f;
    eng.camPos[1] = (float)get_height(0, 0) + 2.5f;

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
