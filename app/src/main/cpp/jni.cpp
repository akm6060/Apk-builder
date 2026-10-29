// JNI bridge + AAudio low-latency output stream.
#include <jni.h>
#include <aaudio/AAudio.h>
#include <android/log.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <algorithm>
#include "engine.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "SynthKB", __VA_ARGS__)
static sk::Engine gEng;
static AAudioStream* gSt = nullptr;
static std::mutex gMu;
static std::atomic<bool> gWant{false}, gRestarting{false};
static int32_t gBurst = 192, gLastXr = 0;

static aaudio_data_callback_result_t dataCb(AAudioStream* s, void*, void* data, int32_t frames) {
    gEng.render((float*)data, frames);
    // grow the buffer by one burst when an underrun is reported (keeps latency minimal, avoids crackle)
    int32_t xr = AAudioStream_getXRunCount(s);
    if (xr > gLastXr) {
        gLastXr = xr;
        int32_t cur = AAudioStream_getBufferSizeInFrames(s), cap = AAudioStream_getBufferCapacityInFrames(s);
        if (cur + gBurst <= std::min(cap, gBurst * 8)) AAudioStream_setBufferSizeInFrames(s, cur + gBurst);
    }
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}
static void closeLocked() { if (gSt) { AAudioStream_requestStop(gSt); AAudioStream_close(gSt); gSt = nullptr; } }
static bool openLocked() {
    if (gSt) return true;
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
        AAudioStreamBuilder_setDataCallback(b, dataCb, nullptr);
        AAudioStreamBuilder_setErrorCallback(b, [](AAudioStream*, void*, aaudio_result_t) {
            if (!gWant || gRestarting.exchange(true)) return;
            std::thread([] { std::lock_guard<std::mutex> l(gMu); closeLocked(); if (gWant) openLocked(); gRestarting = false; }).detach();
        }, nullptr);
        AAudioStream* s = nullptr;
        aaudio_result_t r = AAudioStreamBuilder_openStream(b, &s);
        AAudioStreamBuilder_delete(b);
        if (r != AAUDIO_OK) { LOGI("open failed %d (attempt %d)", r, attempt); continue; }
        gEng.init((float)AAudioStream_getSampleRate(s));
        gBurst = AAudioStream_getFramesPerBurst(s); gLastXr = 0;
        AAudioStream_setBufferSizeInFrames(s, gBurst * 2);
        gSt = s;
        if (AAudioStream_requestStart(s) != AAUDIO_OK) { closeLocked(); continue; }
        LOGI("audio: rate=%d burst=%d buf=%d sharing=%d", AAudioStream_getSampleRate(s), gBurst, AAudioStream_getBufferSizeInFrames(s), AAudioStream_getSharingMode(s));
        return true;
    }
    return false;
}

extern "C" {
#define FN(name) Java_com_synthkeyboard_app_NativeAudio_##name
JNIEXPORT jboolean JNICALL FN(start)(JNIEnv*, jclass) { std::lock_guard<std::mutex> l(gMu); gWant = true; return openLocked(); }
JNIEXPORT void JNICALL FN(stop)(JNIEnv*, jclass) { std::lock_guard<std::mutex> l(gMu); gWant = false; gEng.allOff(); closeLocked(); }
JNIEXPORT void JNICALL FN(noteOn)(JNIEnv* e, jclass, jint id, jfloatArray a, jfloat ratio) {
    float s[sk::NSOUND] = {0}; jsize n = e->GetArrayLength(a); e->GetFloatArrayRegion(a, 0, std::min<jsize>(n, sk::NSOUND), s); gEng.noteOn(id, s, ratio);
}
JNIEXPORT void JNICALL FN(noteOff)(JNIEnv*, jclass, jint id) { gEng.noteOff(id); }
JNIEXPORT void JNICALL FN(cut)(JNIEnv*, jclass, jint id) { gEng.cut(id); }
JNIEXPORT void JNICALL FN(allOff)(JNIEnv*, jclass) { gEng.allOff(); }
JNIEXPORT void JNICALL FN(setGlobal)(JNIEnv* e, jclass, jfloatArray a) {
    float g[sk::NGLOB] = {0}; jsize n = e->GetArrayLength(a); e->GetFloatArrayRegion(a, 0, std::min<jsize>(n, sk::NGLOB), g); gEng.setGlobal(g);
}
JNIEXPORT void JNICALL FN(setDrone)(JNIEnv*, jclass, jboolean on, jfloat base) { gEng.setDrone(on, base); }
JNIEXPORT void JNICALL FN(setReverb)(JNIEnv*, jclass, jfloat d, jfloat p) { gEng.setReverb(d, p); }
}
