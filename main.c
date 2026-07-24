#include <android_native_app_glue.h>
#include <android/native_window.h>
#include <android/log.h>
#include <jni.h> // Обязательно для работы с буфером
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include "engine.h"

extern void graphics_clear(RenderBuffer* rb, uint32_t color);
extern void draw_software_block(struct engine* eng, int bx, int by, int bz, uint32_t color);

static struct engine* g_ptr = NULL;

// ГЕНИАЛЬНАЯ ФУНКЦИЯ КОПИРОВАНИЯ В БУФЕР ОБМЕНА
void copy_to_clipboard(struct android_app* app, const char* text) {
    JNIEnv* env = NULL;
    (*app->activity->vm)->AttachCurrentThread(app->activity->vm, &env, NULL);

    jobject activity = app->activity->clazz;
    jclass context_cls = (*env)->FindClass(env, "android/content/Context");
    jmethodID get_system_service = (*env)->GetMethodID(env, context_cls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    
    // Получаем ClipboardManager
    jstring service_name = (*env)->NewStringUTF(env, "clipboard");
    jobject clipboard_mgr = (*env)->CallObjectMethod(env, activity, get_system_service, service_name);

    // Создаем ClipData через ClipData.newPlainText("label", "text")
    jclass clip_data_cls = (*env)->FindClass(env, "android/content/ClipData");
    jmethodID new_plain_text = (*env)->GetStaticMethodID(env, clip_data_cls, "newPlainText", "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");
    
    jstring label = (*env)->NewStringUTF(env, "Game Error");
    jstring jtext = (*env)->NewStringUTF(env, text);
    jobject clip_data = (*env)->CallStaticObjectMethod(env, clip_data_cls, new_plain_text, label, jtext);

    // Вызываем clipboard_mgr.setPrimaryClip(clip_data)
    jclass clipboard_mgr_cls = (*env)->FindClass(env, "android/content/ClipboardManager");
    jmethodID set_primary_clip = (*env)->GetMethodID(env, clipboard_mgr_cls, "setPrimaryClip", "(Landroid/content/ClipData;)V");
    
    (*env)->CallVoidMethod(env, clipboard_mgr, set_primary_clip, clip_data);

    (*app->activity->vm)->DetachCurrentThread(app->activity->vm);
}

void handle_fatal(int sig) {
    if (g_ptr) {
        char msg[256];
        sprintf(msg, "CRASH: Signal %d (Segmentation Fault) at block render", sig);
        // Копируем в буфер обмена!
        copy_to_clipboard(g_eng->app, msg);
        longjmp(g_ptr->crash_env, 1);
    }
    exit(sig);
}

void android_main(struct android_app* s) {
    struct engine* e = (struct engine*)malloc(sizeof(struct engine));
    memset(e, 0, sizeof(struct engine));
    e->app = s;
    g_ptr = e;

    signal(SIGSEGV, handle_fatal);
    signal(SIGFPE, handle_fatal);

    // Генерация мира
    for(int x=0; x<WORLD_BUF; x++) for(int z=0; z<WORLD_BUF; z++) e->blocks[x][0][z] = 1;
    e->camPos[0]=8; e->camPos[1]=4; e->camPos[2]=8;

    while (1) {
        int ev;
        struct android_poll_source* src;
        while (ALooper_pollOnce(0, 0, &ev, (void**)&src) >= 0) {
            if (src) src->process(s, src);
            if (s->destroyRequested) return;
        }

        if (setjmp(e->crash_env) == 0) {
            if (s->window) {
                ANativeWindow_Buffer win;
                if (ANativeWindow_lock(s->window, &win, NULL) >= 0) {
                    e->rb.pixels = (uint32_t*)win.bits;
                    e->rb.width = win.width; e->rb.height = win.height; e->rb.stride = win.stride;
                    if (!e->rb.z_buffer) e->rb.z_buffer = malloc(e->rb.width * e->rb.height * sizeof(float));

                    graphics_clear(&e->rb, 0xFF88CCFF);
                    for(int x=0; x<WORLD_BUF; x++) for(int z=0; z<WORLD_BUF; z++) {
                        draw_software_block(e, x, 0, z, 0xFF00AA00);
                    }
                    ANativeWindow_unlockAndPost(s->window);
                }
            }
        } else {
            // ЭТО СРАБОТАЕТ ПРИ ВЫЛЕТЕ
            // Ошибка уже в буфере обмена благодаря handle_fatal
            usleep(1000000); // Тормозим на 1 секунду
        }
    }
}
