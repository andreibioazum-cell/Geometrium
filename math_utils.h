#ifndef MATH_UTILS_H
#define MATH_UTILS_H
#include <math.h>
#include <string.h>

void mat4_identity(float* m) {
    memset(m, 0, 64); m[0]=m[5]=m[10]=m[15]=1.0f;
}

void mat4_perspective(float* m, float fov, float aspect, float n, float f) {
    float S = 1.0f / tanf(fov * 0.5f);
    memset(m, 0, 64);
    m[0] = S / aspect;
    m[5] = S;
    m[10] = (f + n) / (n - f);
    m[11] = -1.0f;
    m[14] = (2.0f * f * n) / (n - f);
}

void mat4_rotate(float* m, float angle) {
    mat4_identity(m);
    m[0] = cosf(angle); m[2] = sinf(angle);
    m[8] = -sinf(angle); m[10] = cosf(angle);
    m[5] = cosf(angle * 0.5f); m[6] = sinf(angle * 0.5f); // Добавим вращение и по X
}

void mat4_mul(float* out, float* a, float* b) {
    float res[16];
    for (int c=0; c<4; c++) {
        for (int r=0; r<4; r++) {
            res[c*4+r] = a[0*4+r]*b[c*4+0] + a[1*4+r]*b[c*4+1] + a[2*4+r]*b[c*4+2] + a[3*4+r]*b[c*4+3];
        }
    }
    memcpy(out, res, 64);
}
#endif
