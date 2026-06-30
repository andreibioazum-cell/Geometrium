#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdio.h>

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
    glClearColor(0.53f, 0.81f, 0.98f, 1.0f);
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
        EGLConfig config; EGLint n;
        EGLint att[] = {EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_DEPTH_SIZE,16,EGL_NONE};
        eglChooseConfig(eng->display, att, &config, 1, &n);
        eng->surface = eglCreateWindowSurface(eng->display, config, eng->app->window, NULL);
        EGLint ctxAtt[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
        eng->context = eglCreateContext(eng->display, config, NULL, ctxAtt);
        eglMakeCurrent(eng->display, eng->surface, eng->surface, eng->context);

        const char* vS =
            "attribute vec3 pos;"
            "attribute vec2 uv;"
            "attribute vec3 norm;"
            "attribute float blockType;"
            "varying vec2 vUV;"
            "varying vec3 vNorm;"
            "varying vec3 vWorldPos;"
            "varying float vBlock;"
            "uniform mat4 m;"
            "void main(){"
            "  gl_Position = m * vec4(pos, 1.0);"
            "  vUV = uv;"
            "  vNorm = norm;"
            "  vWorldPos = pos;"
            "  vBlock = blockType;"
            "}";

        /* Шейдер с текстурами по типу блока и грани */
        const char* fS =
            "precision mediump float;"
            "varying vec2 vUV;"
            "varying vec3 vNorm;"
            "varying vec3 vWorldPos;"
            "varying float vBlock;"
            "uniform vec3 camPos;"
            "uniform sampler2D texT1,texS1,texB1;"
            "uniform sampler2D texT2,texS2,texB2;"
            "uniform sampler2D texT3,texS3,texB3;"
            "uniform sampler2D texT4,texS4,texB4;"
            "uniform sampler2D texT5,texS5,texB5;"
            "void main(){"
            "  vec4 texCol;"
            "  int bt = int(vBlock + 0.5);"
            "  bool isTop = vNorm.y > 0.5;"
            "  bool isBot = vNorm.y < -0.5;"
            "  if(bt==1){"
            "    if(isTop) texCol=texture2D(texT1,vUV);"
            "    else if(isBot) texCol=texture2D(texB1,vUV);"
            "    else texCol=texture2D(texS1,vUV);"
            "  }else if(bt==2){"
            "    if(isTop) texCol=texture2D(texT2,vUV);"
            "    else if(isBot) texCol=texture2D(texB2,vUV);"
            "    else texCol=texture2D(texS2,vUV);"
            "  }else if(bt==3){"
            "    if(isTop) texCol=texture2D(texT3,vUV);"
            "    else if(isBot) texCol=texture2D(texB3,vUV);"
            "    else texCol=texture2D(texS3,vUV);"
            "  }else if(bt==4){"
            "    if(isTop) texCol=texture2D(texT4,vUV);"
            "    else if(isBot) texCol=texture2D(texB4,vUV);"
            "    else texCol=texture2D(texS4,vUV);"
            "  }else{"
            "    if(isTop) texCol=texture2D(texT5,vUV);"
            "    else if(isBot) texCol=texture2D(texB5,vUV);"
            "    else texCol=texture2D(texS5,vUV);"
            "  }"
            "  vec3 lightDir=normalize(vec3(0.4,0.9,0.3));"
            "  float diff=max(dot(vNorm,lightDir),0.0);"
            "  float ambient=0.45;"
            "  float faceBias=0.0;"
            "  if(vNorm.y>0.5)faceBias=0.1;"
            "  if(vNorm.y<-0.5)faceBias=-0.15;"
            "  if(abs(vNorm.x)>0.5)faceBias=-0.05;"
            "  if(abs(vNorm.z)>0.5)faceBias=-0.1;"
            "  float light=clamp(ambient+diff*0.55+faceBias,0.2,1.0);"
            "  vec3 toFrag=vWorldPos-camPos;"
            "  float dist=length(toFrag.xz);"
            "  vec3 fogCol=vec3(0.53,0.81,0.98);"
            "  float fog=clamp((dist-20.0)/25.0,0.0,0.85);"
            "  vec3 col=mix(texCol.rgb*light,fogCol,fog);"
            "  gl_FragColor=vec4(col,1.0);"
            "}";

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs,1,&vS,NULL); glCompileShader(vs);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs,1,&fS,NULL); glCompileShader(fs);
        eng->program = glCreateProgram();
        glBindAttribLocation(eng->program, 0, "pos");
        glBindAttribLocation(eng->program, 1, "uv");
        glBindAttribLocation(eng->program, 2, "norm");
        glBindAttribLocation(eng->program, 3, "blockType");
        glAttachShader(eng->program, vs);
        glAttachShader(eng->program, fs);
        glLinkProgram(eng->program);
        glDeleteShader(vs); glDeleteShader(fs);

        init_block_textures(eng);
        init_ui_shader();
    }
}

void android_main(struct android_app* state) {
    struct engine eng = {0};
    eng.movePointerId = -1;
    eng.lookPointerId = -1;
    eng.worldLoaded = false;
    eng.meshDirty = true;
    eng.selectedSlot = 0;

    /* Инвентарь */
    eng.invSlots[0] = BLOCK_GRASS;
    eng.invSlots[1] = BLOCK_DIRT;
    eng.invSlots[2] = BLOCK_STONE;
    eng.invSlots[3] = BLOCK_SAND;
    eng.invSlots[4] = BLOCK_SNOW;

    eng.camPos[0] = 0.5f;
    eng.camPos[2] = -0.5f;
    eng.camPos[1] = (float)get_height(0, 0) + 2.5f;

    state->userData = &eng;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;
    eng.app = state;

    while (1) {
        int ev; struct android_poll_source* s;
        while (ALooper_pollOnce(0, NULL, &ev, (void**)&s) >= 0) {
            if (s) s->process(state, s);
            if (state->destroyRequested) return;
        }
        engine_draw_frame(&eng);
    }
}
