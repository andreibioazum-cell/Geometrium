#ifndef MATH_UTILS_H
#define MATH_UTILS_H
#include <math.h>
#include <string.h>

void mat4_identity(float* m) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void mat4_perspective(float* m, float fov, float aspect, float near, float far) {
    float f = 1.0f / tanf(fov * 0.5f);
    memset(m, 0, 16 * sizeof(float));
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (far + near) / (near - far);
    m[11] = -1.0f;
    m[14] = (2.0f * far * near) / (near - far);
}

void mat4_rotate_y(float* m, float angle) {
    mat4_identity(m);
    m[0] = cosf(angle);
    m[2] = -sinf(angle);
    m[8] = sinf(angle);
    m[10] = cosf(angle);
}

void mat4_translate(float* m, float x, float y, float z) {
    mat4_identity(m);
    m[12] = x; m[13] = y; m[14] = z;
}

void mat4_mul(float* res, float* a, float* b) {
    float tmp[16];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            tmp[i*4+j] = a[i*4+0]*b[0*4+j] + a[i*4+1]*b[1*4+j] + a[i*4+2]*b[2*4+j] + a[i*4+3]*b[3*4+j];
        }
    }
    memcpy(res, tmp, 16 * sizeof(float));
}
#endif
