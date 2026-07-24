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

// Внешние функции из твоего graphics.c
extern void graphics_clear(RenderBuffer* rb, uint32_t color);
extern void draw_software_block(struct engine* eng, int bx, int by, int bz, uint32_t color);

// Глобальные переменные для перехвата
static sigjmp_buf g_jump_buf;
static volatile sig_atomic_t g_error_occurred = 0;
static char g_error_msg[256];

// Флаг, что окно заблокировано (чтобы не было дедлока при вылете)
static bool g_is_locked = false;

// ФУНКЦИЯ КОПИРОВАНИЯ (сверхбезопасная)
void copy_to_clipboard(struct android_app* app, const char* text) {
    __android_log_print(ANDROID_LOG_INFO, "GEOMETRIUM", "Attempting to copy error to clipboard...");
    
    JNIEnv* env = NULL;
    JavaVM* vm = app->activity->vm;
    (*vm)->AttachCurrentThread(vm, &env, NULL);

    jobject activity = app->activity->clazz;
    
    // Получаем ClipboardManager через Context.CLIPBOARD_SERVICE
    jclass context_cls = (*env)->FindClass(env, "android/content/Context");
    jfieldID service_field = (*env)->GetStaticFieldID(env, context_cls, "CLIPBOARD_SERVICE", "Ljava/lang/String;");
    jstring service_name = (jstring)(*env)->GetStaticObjectField(env, context_cls, service_field);
    
    jmethodID get_ss = (*env)->GetMethodID(env, context_cls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jobject clipboard_mgr = (*env)->CallObjectMethod(env, activity, get_ss, service_name);

    if (clipboard_mgr) {
        jclass clip_data_cls = (*env)->FindClass(env, "android/content/ClipData");
        jmethodID new_text = (*env)->GetStaticMethodID(env, clip_data_cls, "newPlainText", "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");
        
        jstring label = (*env)->NewStringUTF(env, "CrashLog");
        jstring content = (*env)->NewStringUTF(env, text);
        jobject clip_data = (*env)->CallStaticObjectMethod(env, clip_data_cls, new_text, label, content);

        jclass cb_mgr_cls = (*env)->FindClass(env, "android/content/ClipboardManager");
        jmethodID set_clip = (*env)->GetMethodID(env, cb_mgr_cls, "setPrimaryClip", "(Landroid/content/ClipData;)V");
        (*env)->CallVoidMethod(env, clipboard_mgr, set_clip, clip_data);
        
        __android_log_print(ANDROID_LOG_INFO, "GEOMETRIUM", "Copy successful!");
    }

    (*vm)->DetachCurrentThread(vm);
}

// Обработчик сигналов
void fatal_handler(int sig) {
    g_error_occurred = 1;
    sprintf(g_error_msg, "CRASH: Signal %d. Last Pos: %.1f", sig, 0.0f); // Можно добавить координаты
    siglongjmp(g_jump_buf, 1);
}

void android_main(struct android_app* s) {
    struct engine* e = (struct engine*)malloc(sizeof(struct engine));
    memset(e, 0, sizeof(struct engine));
    e->app = s;
    s->userData = e;

    // НАСТРОЙКА СИГНАЛОВ
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = fatal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NODEFER | SA_RESTART; // Важно для повторных срабатываний
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);

    while (1) {
        int ev;
        struct android_poll_source* src;
        while (ALooper_pollOnce(0, 0, &ev, (void**)&src) >= 0) {
            if (src) src->process(s, src);
            if (s->destroyRequested) return;
        }

        // --- ТОЧКА ПРЫЖКА (pcall) ---
        if (sigsetjmp(g_jump_buf, 1) == 0) {
            if (s->window) {
                ANativeWindow_Buffer win;
                if (ANativeWindow_lock(s->window, &win, NULL) >= 0) {
                    g_is_locked = true;
                    
                    e->rb.pixels = (uint32_t*)win.bits;
                    e->rb.width = win.width; e->rb.stride = win.stride; e->rb.height = win.height;
                    if (!e->rb.z_buffer) e->rb.z_buffer = malloc(win.width * win.height * sizeof(float));

                    graphics_clear(&e->rb, 0xFF88CCFF);
                    
                    // ТЕСТ ВЫЛЕТА (удали потом)
                    // if (e->camPos[0] == 0) { int* p = NULL; *p = 1; }

                    for(int x=0; x<WORLD_BUF; x++) {
                        for(int z=0; z<WORLD_BUF; z++) {
                            draw_software_block(e, x, 0, z, 0xFF00AA00);
                        }
                    }

                    ANativeWindow_unlockAndPost(s->window);
                    g_is_locked = false;
                }
            }
        } else {
            // МЫ ВЫЛЕТЕЛИ И ПРЫГНУЛИ СЮДА
            if (g_is_locked && s->window) {
                ANativeWindow_unlockAndPost(s->window); // Разблокируем экран, чтобы не зависло
                g_is_locked = false;
            }

            __android_log_print(ANDROID_LOG_ERROR, "GEOMETRIUM", "CATCHED: %s", g_error_msg);
            
            // Копируем в буфер (теперь безопасно)
            copy_to_clipboard(s, g_error_msg);
            
            sleep(1); // Тормозим, чтобы не зациклило вылеты
        }
    }
}
