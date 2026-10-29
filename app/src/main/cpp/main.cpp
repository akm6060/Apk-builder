// Android glue: NativeActivity + AAudio low-latency output + software-drawn UI.  No Java, no WebView.
#include <android/log.h>
#include <android/native_window.h>
#include <android/window.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android_native_app_glue.h>
#include <aaudio/AAudio.h>
#include <jni.h>
#include <time.h>
#include <algorithm>
#include <atomic>
#include <string>
#include "app.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "SynthKB", __VA_ARGS__)

struct Ctx {
    android_app* app = nullptr;
    Engine engine;
    App* ui = nullptr;
    ANativeWindow* win = nullptr;
    AAudioStream* st = nullptr;
    std::atomic<bool> audErr{false};
    bool resumed = false;
    int32_t lastXr = 0, burst = 192; double lastXrChk = 0, lastDraw = 0;
};

static double nowMs() { timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6; }

// ---------------------------------------------------------------- audio
static aaudio_data_callback_result_t dataCb(AAudioStream*, void* user, void* data, int32_t frames) {
    ((Ctx*)user)->engine.render((float*)data, frames);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}
static void errCb(AAudioStream*, void* user, aaudio_result_t) { ((Ctx*)user)->audErr = true; }

static void stopAudio(Ctx* c) {
    if (c->st) { AAudioStream_requestStop(c->st); AAudioStream_close(c->st); c->st = nullptr; }
}
static bool startAudio(Ctx* c) {
    if (c->st) return true;
    for (int attempt = 0; attempt < 2; attempt++) {
        AAudioStreamBuilder* b = nullptr;
        if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return false;
        AAudioStreamBuilder_setDirection(b, AAUDIO_DIRECTION_OUTPUT);
        AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
        AAudioStreamBuilder_setSharingMode(b, attempt == 0 ? AAUDIO_SHARING_MODE_EXCLUSIVE : AAUDIO_SHARING_MODE_SHARED);
        AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_FLOAT);
        AAudioStreamBuilder_setChannelCount(b, 2);
        AAudioStreamBuilder_setUsage(b, AAUDIO_USAGE_GAME);
        AAudioStreamBuilder_setContentType(b, AAUDIO_CONTENT_TYPE_MUSIC);
        AAudioStreamBuilder_setDataCallback(b, dataCb, c);
        AAudioStreamBuilder_setErrorCallback(b, errCb, c);
        AAudioStream* s = nullptr;
        aaudio_result_t r = AAudioStreamBuilder_openStream(b, &s);
        AAudioStreamBuilder_delete(b);
        if (r != AAUDIO_OK) { LOGI("open failed %d (attempt %d)", r, attempt); continue; }
        c->engine.init((float)AAudioStream_getSampleRate(s));
        c->burst = AAudioStream_getFramesPerBurst(s);
        AAudioStream_setBufferSizeInFrames(s, c->burst * 2);
        c->st = s; c->lastXr = 0; c->audErr = false;
        if (c->ui) c->ui->pubParams();
        if (AAudioStream_requestStart(s) != AAUDIO_OK) { stopAudio(c); continue; }
        LOGI("audio started: rate=%d burst=%d buffer=%d sharing=%d", AAudioStream_getSampleRate(s), c->burst, AAudioStream_getBufferSizeInFrames(s), AAudioStream_getSharingMode(s));
        return true;
    }
    return false;
}
// grow the buffer by one burst whenever an underrun is reported (keeps latency minimal, avoids crackle)
static void adaptBuffer(Ctx* c, double now) {
    if (!c->st || now - c->lastXrChk < 500) return; c->lastXrChk = now;
    int32_t xr = AAudioStream_getXRunCount(c->st);
    if (xr > c->lastXr) {
        int32_t cur = AAudioStream_getBufferSizeInFrames(c->st), mx = AAudioStream_getBufferCapacityInFrames(c->st);
        if (cur + c->burst <= std::min(mx, c->burst * 8)) AAudioStream_setBufferSizeInFrames(c->st, cur + c->burst);
        c->lastXr = xr;
    }
}

// ---------------------------------------------------------------- fullscreen
static void hideSystemUi(ANativeActivity* act) {
    ANativeActivity_setWindowFlags(act, AWINDOW_FLAG_KEEP_SCREEN_ON | AWINDOW_FLAG_FULLSCREEN, 0);
    JavaVM* vm = act->vm; JNIEnv* env = nullptr; bool att = false;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) { if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return; att = true; }
    jclass ac = env->GetObjectClass(act->clazz);
    jmethodID gw = env->GetMethodID(ac, "getWindow", "()Landroid/view/Window;");
    jobject win = gw ? env->CallObjectMethod(act->clazz, gw) : nullptr;
    if (win) {
        jclass wc = env->GetObjectClass(win);
        jmethodID gd = env->GetMethodID(wc, "getDecorView", "()Landroid/view/View;");
        jobject dec = gd ? env->CallObjectMethod(win, gd) : nullptr;
        if (dec) {
            jclass vc = env->GetObjectClass(dec);
            jmethodID sv = env->GetMethodID(vc, "setSystemUiVisibility", "(I)V");
            // LAYOUT_STABLE|LAYOUT_HIDE_NAVIGATION|LAYOUT_FULLSCREEN|HIDE_NAVIGATION|FULLSCREEN|IMMERSIVE_STICKY
            if (sv) env->CallVoidMethod(dec, sv, 0x100 | 0x200 | 0x400 | 0x2 | 0x4 | 0x1000);
        }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (att) vm->DetachCurrentThread();
}

// ---------------------------------------------------------------- lifecycle / input
static void draw(Ctx* c) {
    if (!c->win) return;
    ANativeWindow_Buffer b;
    if (ANativeWindow_lock(c->win, &b, nullptr) == 0) {
        c->ui->draw((uint32_t*)b.bits, b.stride, b.width, b.height);
        ANativeWindow_unlockAndPost(c->win);
    }
}
static void onCmd(android_app* a, int32_t cmd) {
    Ctx* c = (Ctx*)a->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            c->win = a->window;
            if (c->win) { ANativeWindow_setBuffersGeometry(c->win, 0, 0, WINDOW_FORMAT_RGBA_8888); c->ui->resize(ANativeWindow_getWidth(c->win), ANativeWindow_getHeight(c->win)); hideSystemUi(a->activity); }
            break;
        case APP_CMD_TERM_WINDOW: c->win = nullptr; break;
        case APP_CMD_WINDOW_RESIZED: case APP_CMD_CONFIG_CHANGED:
            if (c->win) c->ui->resize(ANativeWindow_getWidth(c->win), ANativeWindow_getHeight(c->win));
            break;
        case APP_CMD_GAINED_FOCUS: hideSystemUi(a->activity); break;
        case APP_CMD_LOST_FOCUS: c->ui->releaseAll(); break;
        case APP_CMD_RESUME: c->resumed = true; startAudio(c); break;
        case APP_CMD_PAUSE: c->resumed = false; c->ui->releaseAll(); c->ui->save(); stopAudio(c); break;
        default: break;
    }
}
static int32_t onInput(android_app* a, AInputEvent* e) {
    Ctx* c = (Ctx*)a->userData;
    if (AInputEvent_getType(e) == AINPUT_EVENT_TYPE_MOTION) {
        int32_t act = AMotionEvent_getAction(e), m = act & AMOTION_EVENT_ACTION_MASK;
        int idx = (act & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        size_t n = AMotionEvent_getPointerCount(e); Pt pts[20]; int cnt = 0;
        for (size_t i = 0; i < n && cnt < 20; i++) {
            if ((m == AMOTION_EVENT_ACTION_UP || m == AMOTION_EVENT_ACTION_POINTER_UP) && (int)i == idx) continue;
            pts[cnt++] = Pt{AMotionEvent_getPointerId(e, i), AMotionEvent_getX(e, i), AMotionEvent_getY(e, i)};
        }
        c->ui->onTouch(pts, cnt, m == AMOTION_EVENT_ACTION_CANCEL, nowMs());
        return 1;
    }
    if (AInputEvent_getType(e) == AINPUT_EVENT_TYPE_KEY && AKeyEvent_getKeyCode(e) == AKEYCODE_BACK) {
        if (AKeyEvent_getAction(e) == AKEY_EVENT_ACTION_UP) ANativeActivity_finish(a->activity);
        return 1;
    }
    return 0;
}

void android_main(android_app* app) {
    Ctx* c = new Ctx(); c->app = app; app->userData = c; app->onAppCmd = onCmd; app->onInputEvent = onInput;
    c->engine.init(48000);
    c->ui = new App(c->engine, app->activity->internalDataPath ? app->activity->internalDataPath : "/data/local/tmp");
    while (true) {
        double now = nowMs();
        int timeout = c->ui->needTick() ? 8 : -1;
        if (c->win && c->ui->dirty()) timeout = std::max(0, (int)(10 - (now - c->lastDraw)));
        int ev; android_poll_source* src;
        while (ALooper_pollOnce(timeout, nullptr, &ev, (void**)&src) >= 0) {
            if (src) src->process(app, src);
            if (app->destroyRequested) { c->ui->save(); stopAudio(c); return; }
            timeout = 0;
        }
        now = nowMs();
        c->ui->tick(now);
        if (c->audErr.exchange(false) && c->resumed) { stopAudio(c); startAudio(c); }
        if (c->resumed && !c->st) startAudio(c);
        adaptBuffer(c, now);
        if (c->win && c->ui->dirty() && now - c->lastDraw >= 10) { draw(c); c->lastDraw = now; }
    }
}
