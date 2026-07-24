#include <android_native_app_glue.h>
#include <android/native_window.h>
#include <android/log.h>
#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include "engine.h"

// Внешние функции из graphics.c
extern void graphics_clear(RenderBuffer* rb, uint32_t color);
extern void draw_software_block(struct engine* eng, int bx, int by, int bz, uint32_t color);

// Глобальный указатель для обработчика сигналов
static struct engine* g_eng = NULL;

// Функция копирования в буфер обмена Android через JNI
void copy_to_clipboard(struct android_app* app, const char* text) {
    if (!app || !app->activity || !app->activity->vm) return;
    
    JNIEnv* env = NULL;
    JavaVM* vm = app->activity->vm;
    (*vm)->AttachCurrentThread(vm, &env, NULL);

    jobject activity = app->activity->clazz;
    jclass context_cls = (*env)->FindClass(env, "android/content/Context");
    jmethodID get_system_service = (*env)->GetMethodID(env, context_cls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    
    jstring service_name = (*env)->NewStringUTF(env, "clipboard");
    jobject clipboard_mgr = (*env)->CallObjectMethod(env, activity, get_system_service, service_name);

    jclass clip_data_cls = (*env)->FindClass(env, "android/content/ClipData");
    jmethodID new_plain_text = (*env)->GetStaticMethodID(env, clip_data_cls, "newPlainText", "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");
    
    jstring label = (*env)->NewStringUTF(env, "Geometrium Error");
    jstring jtext = (*env)->NewStringUTF(env, text);
    jobject clip_data = (*env)->CallStaticObjectMethod(env, clip_data_cls, new_plain_text, label, jtext);

    jclass clipboard_mgr_cls = (*env)->FindClass(env, "android/content/ClipboardManager");
    jmethodID set_primary_clip = (*env)->GetMethodID(env, clipboard_mgr_cls, "setPrimaryClip", "(Landroid/content/ClipData;)V");
    
    (*env)->CallVoidMethod(env, clipboard_mgr, set_primary_clip, clip_data);

    (*vm)->DetachCurrentThread(vm);
}

// Обработчик вылетов
void handle_fatal(int sig) {
    if (g_eng) {
        char msg[256];
        sprintf(msg, "FATAL ERROR: Signal %d (Segmentation Fault). Координаты: X=%.2f Y=%.2f Z=%.2f", 
                sig, g_eng->camPos[0], g_eng->camPos[1], g_eng->camPos[2]);
        
        // Копируем текст ошибки в буфер обмена телефона
        copy_to_clipboard(g_eng->app, msg);
        
        // Прыгаем обратно в безопасную точку цикла
        longjmp(g_eng->crash_env, 1);
    }
    exit(sig);
}

void android_main(struct android_app* s) {
    struct engine* e = (struct engine*)malloc(sizeof(struct engine));
    if (!e) return;
    memset(e, 0, sizeof(struct engine));
    
    e->app = s;
    g_eng = e; // Присваиваем глобальной переменной

    // Установка перехватчиков вылета
    signal(SIGSEGV, handle_fatal);
    signal(SIGFPE, handle_fatal);

    // Генерация тестового пола
    for(int x=0; x<WORLD_BUF; x++) {
        for(int z=0; z<WORLD_BUF; z++) {
            e->blocks[x][0][z] = 1;
        }
    }
    e->camPos[0]=8; e->camPos[1]=5; e->camPos[2]=8;

    while (1) {
        int ev;
        struct android_poll_source* src;
        while (ALooper_pollOnce(0, 0, &ev, (void**)&src) >= 0) {
            if (src) src->process(s, src);
            if (s->destroyRequested) return;
        }

        // ТОЧКА ВОЗВРАТА (pcall)
        if (setjmp(e->crash_env) == 0) {
            if (s->window) {
                ANativeWindow_Buffer win;
                if (ANativeWindow_lock(s->window, &win, NULL) >= 0) {
                    e->rb.pixels = (uint32_t*)win.bits;
                    e->rb.width = win.width; 
                    e->rb.height = win.height; 
                    e->rb.stride = win.stride;

                    if (!e->rb.z_buffer) {
                        e->rb.z_buffer = malloc(e->rb.width * e->rb.height * sizeof(float));
                    }

                    // Очистка и рендер (всё на CPU)
                    graphics_clear(&e->rb, 0xFF88CCFF);
                    for(int x=0; x<WORLD_BUF; x++) {
                        for(int z=0; z<WORLD_BUF; z++) {
                            draw_software_block(e, x, 0, z, 0xFF00AA00);
                        }
                    }
                    ANativeWindow_unlockAndPost(s->window);
                }
            }
        } else {
            // Сюда программа прыгнет ПРИ ВЫЛЕТЕ
            // Ошибка уже в буфере обмена, просто ждем секунду
            usleep(1000000); 
        }
    }
}
