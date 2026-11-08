// Oboe low-latency output stream + JNI entry points for com.synthkeyboard.app.NativeAudio
// (same stream set-up as the Harmonium app: shared low-latency path with the phone's normal speaker processing by default)
#include <jni.h>
#include <oboe/Oboe.h>
#include <android/log.h>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <mutex>
#include "engine.h"

using namespace oboe;

namespace {

sk::Engine gEngine;
std::mutex gStreamLock;
/* 0 = Fast (shared low-latency, normal speaker processing)  1 = Fast exclusive (lowest delay, raw speaker)
   2 = Rich (phone's normal sound path; more delay) */
int gOutMode = 0;
bool gWanted = false;

class Player : public AudioStreamDataCallback, public AudioStreamErrorCallback {
  public:
    std::shared_ptr<AudioStream> stream;
    std::unique_ptr<LatencyTuner> tuner;
    bool exclusive = false;

    DataCallbackResult onAudioReady(AudioStream*, void* data, int32_t n) override {
        gEngine.render(static_cast<float*>(data), n);
        if (tuner) tuner->tune();
        return DataCallbackResult::Continue;
    }
    // headphones plugged / unplugged, Bluetooth etc.: reopen on the new device
    void onErrorAfterClose(AudioStream*, Result err) override {
        if (err == Result::ErrorDisconnected && gWanted) open();
    }

    bool open() {
        std::lock_guard<std::mutex> lk(gStreamLock);
        if (stream) { stream->close(); stream.reset(); }
        for (int attempt = 0; attempt < 2; attempt++) {
            AudioStreamBuilder b;
            b.setDirection(Direction::Output)
                ->setPerformanceMode(gOutMode == 2 ? PerformanceMode::None : PerformanceMode::LowLatency)
                ->setSharingMode(attempt == 0 && gOutMode == 1 ? SharingMode::Exclusive : SharingMode::Shared)
                ->setFormat(AudioFormat::Float)
                ->setChannelCount(ChannelCount::Stereo)
                ->setUsage(gOutMode == 1 ? Usage::Game : Usage::Media)
                ->setContentType(ContentType::Music)
                ->setDataCallback(this)
                ->setErrorCallback(this);
            Result r = b.openStream(stream);
            if (r == Result::OK && stream) break;
            stream.reset();
        }
        if (!stream) return false;
        exclusive = stream->getSharingMode() == SharingMode::Exclusive;
        gEngine.init((float)stream->getSampleRate());
        // two bursts of buffering to start; the tuner adds more only if the phone actually glitches
        if (gOutMode != 2) {
            stream->setBufferSizeInFrames(stream->getFramesPerBurst() * 2);
            tuner.reset(new LatencyTuner(*stream));
        } else tuner.reset();
        return stream->requestStart() == Result::OK;
    }
    void stopAll() {
        std::lock_guard<std::mutex> lk(gStreamLock);
        if (stream) { stream->stop(); stream->close(); stream.reset(); }
    }
    void pause() { std::lock_guard<std::mutex> lk(gStreamLock); if (stream) stream->requestStop(); }
    bool resume() {
        { std::lock_guard<std::mutex> lk(gStreamLock); if (stream) return stream->requestStart() == Result::OK; }
        return open();
    }
};

Player gPlayer;

}  // namespace

extern "C" {
#define FN(name) Java_com_synthkeyboard_app_NativeAudio_##name

JNIEXPORT jboolean JNICALL FN(start)(JNIEnv*, jclass) { gWanted = true; return gPlayer.resume() ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT void JNICALL FN(stop)(JNIEnv*, jclass) { gWanted = false; gEngine.allOff(); gPlayer.stopAll(); }
JNIEXPORT void JNICALL FN(pause)(JNIEnv*, jclass) { gEngine.allOff(); gPlayer.pause(); }
JNIEXPORT jboolean JNICALL FN(setOutput)(JNIEnv*, jclass, jint m) {
    int n = m < 0 || m > 2 ? 0 : (int)m;
    if (n == gOutMode && gPlayer.stream) return JNI_TRUE;
    gOutMode = n; return gPlayer.open() ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT void JNICALL FN(noteOn)(JNIEnv* e, jclass, jint id, jfloatArray a, jfloat ratio) {
    float s[sk::NSOUND] = {0}; jsize n = e->GetArrayLength(a); e->GetFloatArrayRegion(a, 0, std::min<jsize>(n, sk::NSOUND), s); gEngine.noteOn(id, s, ratio);
}
JNIEXPORT void JNICALL FN(noteOff)(JNIEnv*, jclass, jint id) { gEngine.noteOff(id); }
JNIEXPORT void JNICALL FN(cut)(JNIEnv*, jclass, jint id) { gEngine.cut(id); }
JNIEXPORT void JNICALL FN(allOff)(JNIEnv*, jclass) { gEngine.allOff(); }
JNIEXPORT void JNICALL FN(setGlobal)(JNIEnv* e, jclass, jfloatArray a) {
    float g[sk::NGLOB] = {0}; jsize n = e->GetArrayLength(a); e->GetFloatArrayRegion(a, 0, std::min<jsize>(n, sk::NGLOB), g); gEngine.setGlobal(g);
}
JNIEXPORT void JNICALL FN(setDrone)(JNIEnv*, jclass, jboolean on, jfloat base) { gEngine.setDrone(on, base); }
JNIEXPORT void JNICALL FN(setReverb)(JNIEnv*, jclass, jfloat d, jfloat p) { gEngine.setReverb(d, p); }

JNIEXPORT jstring JNICALL FN(info)(JNIEnv* env, jclass) {
    char b[256];
    std::lock_guard<std::mutex> lk(gStreamLock);
    if (!gPlayer.stream) return env->NewStringUTF("Native audio: not running");
    auto& s = gPlayer.stream;
    auto xr = s->getXRunCount();
    auto lat = s->calculateLatencyMillis();
    snprintf(b, sizeof b, "Oboe (%s, %s, %s): %d Hz, burst %d, buffer %d frames, delay %.0f ms, xruns %d",
             s->getAudioApi() == AudioApi::AAudio ? "AAudio" : "OpenSL ES",
             s->getPerformanceMode() == PerformanceMode::LowLatency ? "low-latency" : "normal",
             gPlayer.exclusive ? "exclusive" : "shared", s->getSampleRate(), s->getFramesPerBurst(),
             s->getBufferSizeInFrames(), lat ? lat.value() : -1.0, xr ? xr.value() : 0);
    return env->NewStringUTF(b);
}
}  // extern "C"
