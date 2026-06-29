// Добавь это в существующий math_utils.h
void mat4_lookat(float* m, float* pos, float pitch, float yaw) {
    float cosP = cosf(pitch), sinP = sinf(pitch);
    float cosY = cosf(yaw), sinY = sinf(yaw);
    
    mat4_identity(m);
    // Матрица вращения камеры (Look)
    float rotX[16], rotY[16], res[16];
    mat4_rotate_x(rotX, pitch);
    mat4_rotate_y(rotY, yaw);
    mat4_mul(res, rotX, rotY);
    
    // Перенос (Move)
    float trans[16];
    mat4_identity(trans);
    trans[12] = -pos[0]; trans[13] = -pos[1]; trans[14] = -pos[2];
    
    mat4_mul(m, res, trans);
}
