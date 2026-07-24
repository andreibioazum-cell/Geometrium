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

extern void graphics_clear(RenderBuffer* rb, uint32_t color);
extern void draw_software_block(struct engine* eng, int bx, int by, int bz, uint32_t color);

static struct engine* g_eng = NULL;
static int last_sig = 0; // Переменная для хранения номера ошибки

// Функция копирования (вызывается ТОЛЬКО из основного цикла)
void copy_to_clipboard(struct android_app* app, const char* text) {
    if (!app || !app->activity || !app->activity->vm) return;
    JNIEnv* env = NULL;
    (*app->activity->vm)->AttachCurrentThread(app->activity->vm, &env, NULL);

    jobject activity = app->activity->clazz;
    jclass context_cls = (*env)->FindClass(env, "android/content/Context");
    jmethodID get_system_service = (*env)->GetMethodID(env, context_cls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jobject clipboard_mgr = (*env)->CallObjectMethod(env, activity, get_system_service, (*env)->NewStringUTF(env, "clipboard"));

    jclass clip_data_cls = (*env)->FindClass(env, "android/content/ClipData");
    jmethodID new_plain_text = (*env)->GetStaticMethodID(env, clip_data_cls, "newPlainText", "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");
    jobject clip_data = (*env)->CallStaticObjectMethod(env, clip_data_cls, new_plain_text, (*env)->NewStringUTF(env, "Error"), (*env)->NewStringUTF(env, text));

    jclass clipboard_mgr_cls = (*env)->FindClass(env, "android/content/ClipboardManager");
    jmethodID set_primary_clip = (*env)->GetMethodID(env, clipboard_mgr_cls, "setPrimaryClip", "(Landroid/content/ClipData;)V");
    (*env)->CallVoidMethod(env, clipboard_mgr, set_primary_clip, clip_data);

    (*app->activity->vm)->DetachCurrentThread(app->activity->vm);
}

// Обработчик сигнала БЕЗ JNI (только прыжок)
void handle_fatal(int sig) {
    last_sig = sig;
    if (g_eng) {
        siglongjmp(g_eng->crash_env, 1); // Используем siglongjmp для сигналов
    }
    _exit(sig);
}

void android_main(struct android_app* s) {
    struct engine* e = (struct engine*)malloc(sizeof(struct engine));
    memset(e, 0, sizeof(struct engine));
    e->app = s;
    g_eng = e;

    // Регистрация сигналов
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_fatal;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);

    while (1) {
        int ev;
        struct android_poll_source* src;
        while (ALooper_pollOnce(0, 0, &ev, (void**)&src) >= 0) {
            if (src) src->process(s, src);
            if (s->destroyRequested) return;
        }

        // РЕАЛЬНЫЙ PCALL
        if (sigsetjmp(e->crash_env, 1) == 0) {
            // ОБЫЧНЫЙ РЕНДЕР
            if (s->window) {
                ANativeWindow_Buffer win;
                if (ANativeWindow_lock(s->window, &win, NULL) >= 0) {
                    e->rb.pixels = (uint32_t*)win.bits;
                    e->rb.width = win.width; e->rb.stride = win.stride; e->rb.height = win.height;
                    if (!e->rb.z_buffer) e->rb.z_buffer = malloc(win.width * win.height * sizeof(float));

                    graphics_clear(&e->rb, 0xFF88CCFF);
                    // Специальный тест вылета (раскомментируй для проверки):
                    // int *crash = NULL; *crash = 123; 
                    
                    for(int x=0; x<WORLD_BUF; x++) for(int z=0; z<WORLD_BUF; z++) {
                        draw_software_block(e, x, 0, z, 0xFF00AA00);
                    }
                    ANativeWindow_unlockAndPost(s->window);
                }
            }
        } else {
            // ВОССТАНОВЛЕНИЕ ПОСЛЕ ВЫЛЕТА
            char err_msg[128];
            sprintf(err_msg, "CRASH DETECTED! Signal: %d. Error copied to clipboard.", last_sig);
            __android_log_print(ANDROID_LOG_ERROR, "GEOMETRIUM", "%s", err_msg);
            
            // Теперь вызываем копирование, так как мы ВНЕ обработчика сигнала
            copy_to_clipboard(s, err_msg);
            
            sleep(1); // Тормозим на секунду
        }
    }
}
