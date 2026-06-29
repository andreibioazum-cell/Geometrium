#ifndef UI_H
#define UI_H

#include <GLES2/gl2.h>
#include <math.h>
#include "engine.h"

static GLuint uiProg = 0;

static void init_ui_shader(void) {
    const char* vS = "attribute vec4 p; void main(){ gl_Position=p; }";
    const char* fS =
        "precision mediump float; uniform vec4 col;"
        "void main(){ gl_FragColor=col; }";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vS, NULL); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fS, NULL); glCompileShader(fs);
    uiProg = glCreateProgram();
    glAttachShader(uiProg, vs);
    glAttachShader(uiProg, fs);
    glLinkProgram(uiProg);
    glDeleteShader(vs);
    glDeleteShader(fs);
}

static void draw_ring(float cx, float cy, float r, float thick,
                      int w, int h,
                      float cr, float cg, float cb, float ca) {
    float ndcX = (cx/w)*2.0f - 1.0f;
    float ndcY = 1.0f - (cy/h)*2.0f;
    float rx_o = (r/w)*2.0f, ry_o = (r/h)*2.0f;
    float rx_i = ((r-thick)/w)*2.0f, ry_i = ((r-thick)/h)*2.0f;

    int segs = 32;
    float verts[(32+1)*4];
    for (int i = 0; i <= segs; i++) {
        float a = (float)i/segs * 2.0f * PI;
        float c = cosf(a), s = sinf(a);
        verts[i*4+0] = ndcX + c*rx_o;
        verts[i*4+1] = ndcY + s*ry_o;
        verts[i*4+2] = ndcX + c*rx_i;
        verts[i*4+3] = ndcY + s*ry_i;
    }
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg,"col"), cr,cg,cb,ca);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, (segs+1)*2);
}

static void draw_circle(float cx, float cy, float r,
                         int w, int h,
                         float cr, float cg, float cb, float ca) {
    float ndcX = (cx/w)*2.0f - 1.0f;
    float ndcY = 1.0f - (cy/h)*2.0f;
    float rx = (r/w)*2.0f, ry = (r/h)*2.0f;

    int segs = 24;
    float verts[(24+2)*2];
    verts[0] = ndcX; verts[1] = ndcY;
    for (int i = 0; i <= segs; i++) {
        float a = (float)i/segs * 2.0f * PI;
        verts[(i+1)*2+0] = ndcX + cosf(a)*rx;
        verts[(i+1)*2+1] = ndcY + sinf(a)*ry;
    }
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg,"col"), cr,cg,cb,ca);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, segs+2);
}

static void draw_ui(struct engine* eng) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float jx = JOY_X_OFFSET;
    float jy = eng->height - JOY_Y_OFFSET;

    // Обводка джойстика
    draw_ring(jx, jy, JOY_RADIUS, 3.0f,
              eng->width, eng->height, 0,0,0, 0.8f);

    // Стик
    float hx = jx + eng->moveDirX * JOY_RADIUS * 0.7f;
    float hy = jy + eng->moveDirZ * JOY_RADIUS * 0.7f;
    draw_circle(hx, hy, STICK_RADIUS,
                eng->width, eng->height, 0,0,0, 0.7f);

    // Кнопка прыжка
    float bx = eng->width - JUMP_BTN_OFFSET;
    float by = eng->height - JUMP_BTN_OFFSET;
    draw_ring(bx, by, JUMP_BTN_SIZE, 3.0f,
              eng->width, eng->height, 0,0,0, 0.8f);

    // Стрелка
    float as = JUMP_BTN_SIZE * 0.4f;
    float nx = (bx/eng->width)*2.0f - 1.0f;
    float ny = 1.0f - (by/eng->height)*2.0f;
    float ax = (as/eng->width)*2.0f;
    float ay = (as/eng->height)*2.0f;
    float arrow[] = { nx, ny+ay, nx-ax, ny-ay*0.5f, nx+ax, ny-ay*0.5f };
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg,"col"), 0,0,0, 0.7f);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, arrow);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

#endif
