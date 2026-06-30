static void draw_ui(struct engine* eng) {
    int sw = eng->width;
    int sh = eng->height;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Прицел — белый крестик */
    float ccx = sw / 2.0f;
    float ccy = sh / 2.0f;
    draw_rect(ccx, ccy, 10.0f, 1.5f, sw, sh, 1, 1, 1, 1.0f);
    draw_rect(ccx, ccy, 1.5f, 10.0f, sw, sh, 1, 1, 1, 1.0f);

    /* Джойстик */
    float jx = JOY_X_OFFSET;
    float jy = sh - JOY_Y_OFFSET;
    draw_ring(jx, jy, JOY_RADIUS, 3.0f, sw, sh, 0, 0, 0, 1.0f);
    float hx = jx + eng->moveDirX * JOY_RADIUS * 0.6f;
    float hy = jy + eng->moveDirZ * JOY_RADIUS * 0.6f;
    draw_circle(hx, hy, STICK_RADIUS, sw, sh, 0, 0, 0, 1.0f);

    /* Прыжок */
    float bx = sw - JUMP_BTN_OFFSET;
    float by = sh - JUMP_BTN_OFFSET;
    draw_ring(bx, by, JUMP_BTN_SIZE, 3.0f, sw, sh, 0, 0, 0, 1.0f);
    float as = JUMP_BTN_SIZE * 0.3f;
    float anx = (bx / sw) * 2.0f - 1.0f;
    float any = 1.0f - (by / sh) * 2.0f;
    float aax = (as / sw) * 2.0f;
    float aay = (as / sh) * 2.0f;
    float arrow[] = {
        anx, any + aay,
        anx - aax, any - aay * 0.5f,
        anx + aax, any - aay * 0.5f
    };
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg, "col"), 0, 0, 0, 1.0f);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, arrow);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    /* Кнопка ломания — X */
    float bbx = sw - BREAK_BTN_X;
    float bby = BREAK_BTN_Y;
    draw_ring(bbx, bby, ACTION_BTN_SIZE, 3.0f, sw, sh, 0, 0, 0, 1.0f);
    float xsz = ACTION_BTN_SIZE * 0.28f;
    float xw = 2.5f;
    draw_rect(bbx, bby, xsz, xw, sw, sh, 0, 0, 0, 1.0f);
    draw_rect(bbx, bby, xw, xsz, sw, sh, 0, 0, 0, 1.0f);

    /* Кнопка ставления — + */
    float pbx = sw - PLACE_BTN_X;
    float pby = PLACE_BTN_Y;
    draw_ring(pbx, pby, ACTION_BTN_SIZE, 3.0f, sw, sh, 0, 0, 0, 1.0f);
    float psz = ACTION_BTN_SIZE * 0.3f;
    float pw = 2.5f;
    draw_rect(pbx, pby, psz, pw, sw, sh, 0, 0, 0, 1.0f);
    draw_rect(pbx, pby, pw, psz, sw, sh, 0, 0, 0, 1.0f);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
