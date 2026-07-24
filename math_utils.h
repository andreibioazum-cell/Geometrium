#ifndef MATH_UTILS_H
#define MATH_UTILS_H
#include <math.h>
#include <string.h>

static void mat4_mul_vec(float* out, float* m, float* v) {
    float x=v[0], y=v[1], z=v[2], w=1.0f;
    out[0] = x*m[0] + y*m[4] + z*m[8] + w*m[12];
    out[1] = x*m[1] + y*m[5] + z*m[9] + w*m[13];
    out[2] = x*m[2] + y*m[6] + z*m[10] + w*m[14];
    float ow = x*m[3] + y*m[7] + z*m[11] + w*m[15];
    if(ow != 0) { out[0]/=ow; out[1]/=ow; out[2]/=ow; }
}

// Функции mat4_perspective, mat4_lookat и прочие берем из твоего предыдущего файла math_utils.h без изменений.
#endif
