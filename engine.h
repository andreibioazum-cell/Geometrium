#ifndef ENGINE_H
#define ENGINE_H

#include <android_native_app_glue.h>
#include <stdint.h>
#include <stdbool.h>
#include <setjmp.h>

#define WORLD_BUF 16
#define CHUNK_H 16

typedef struct {
    uint32_t* pixels;
    float* z_buffer;
    int width, height, stride;
} RenderBuffer;

struct engine {
    struct android_app* app;
    RenderBuffer rb;
    
    float camPos[3], camRot[2];
    uint8_t blocks[WORLD_BUF][CHUNK_H][WORLD_BUF];
    
    // Для защиты от вылетов
    jmp_buf crash_env; 
    char last_error[256];
};

#endif
