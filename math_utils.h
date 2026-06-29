#include <math.h>

void mat4_perspective(float* m, float fov, float aspect, float near, float far) {
    float f = 1.0f / tanf(fov * 0.5f);
    for (int i=0; i<16; i++) m[i] = 0;
    m[0] = f / aspect; m[5] = f; m[10] = (far+near)/(near-far);
    m[11] = -1.0f; m[14] = (2.0f*far*near)/(near-far);
}

void mat4_translate(float* m, float x, float y, float z) {
    for (int i=0; i<16; i++) m[i] = (i%5==0);
    m[12] = x; m[13] = y; m[14] = z;
}
