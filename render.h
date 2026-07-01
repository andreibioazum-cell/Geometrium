#ifndef RENDER_H
#define RENDER_H

#include "engine.h"
#include "math_utils.h"

// Вспомогательная функция для рисования одного куба (для анимации)
static void draw_single_cube(struct engine* eng, float x, float y, float z, float scale, unsigned char type) {
    float s = scale * 0.5f;
    float vertices[] = {
        // Упрощенный массив вершин (только верхняя грань для примера анимации)
        x-s, y+s, z+s, 0,0, 0,1,0,  x+s, y+s, z+s, 1,0, 0,1,0,  x+s, y+s, z-s, 1,1, 0,1,0,
        x-s, y+s, z+s, 0,0, 0,1,0,  x+s, y+s, z-s, 1,1, 0,1,0,  x-s, y+s, z-s, 0,1, 0,1,0
    };
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, vertices);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 32, vertices + 3);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 32, vertices + 5);
    glEnableVertexAttribArray(2);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static void render_anim_block(struct engine* eng) {
    if (!eng->animActive) return;
    
    float t = (float)eng->animTimer;
    float scale = 1.0f;
    
    if (eng->animIsBreak) {
        scale = t / (float)ANIM_BREAK_FRAMES;
    } else {
        // Эффект появления: плавно растет с 0 до 1.0
        float progress = 1.0f - (t / (float)ANIM_PLACE_FRAMES);
        // Пружинный эффект (overshoot)
        scale = progress * 1.1f;
        if (scale > 1.0f) scale = 1.0f;
    }

    glUseProgram(eng->program);
    // Настройка матриц (аналогично основному миру)
    draw_single_cube(eng, eng->animBlockPos[0], eng->animBlockPos[1], eng->animBlockPos[2], scale, eng->animBlockType);
    
    eng->animTimer--;
    if (eng->animTimer <= 0) eng->animActive = false;
}

static void render_world(struct engine* eng) {
    if (eng->meshDirty) {
        update_faces(eng);
        rebuild_vbo(eng); // Функция из твоего старого render.h
    }
    
    glUseProgram(eng->program);
    // ... Настройка Uniforms (MVP, camPos) ...
    
    // Рисуем основной мир
    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
    // ... glVertexAttribPointer ...
    glDrawArrays(GL_TRIANGLES, 0, eng->visibleFaceCount * 6);
    
    // Рисуем анимацию сверху
    render_anim_block(eng);
}

#endif
