// Native audio engine: an exact port of the web version's WebAudio graph.
#pragma once
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>

namespace sk {

constexpr int NVOICE = 32;      // slots (soft limit MAXACT active)
constexpr int MAXACT = 22;
constexpr int NGLOB = 24;
constexpr int NSOUND = 29;      // floats describing one sound (see Engine::noteOn)

// global parameter indices (all "effective" values; JS already applied the on/off toggles)
enum { G_CUT, G_RES, G_LR, G_VIB, G_TRM, G_WOB, G_CH, G_EQ0 /*7*/, G_ECHO = 14, G_ET, G_EFB, G_REV, G_PRE = 18 };

struct OscP { float on, w, f, v, t, c; };
struct SoundP { float mode, a, d, s, r; OscP o[4]; };

struct Biquad {                 // transposed direct form II, double precision
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline float run(float x) {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float)y;
    }
    void set(double B0, double B1, double B2, double A0, double A1, double A2) {
        double i = 1.0 / A0; b0 = B0 * i; b1 = B1 * i; b2 = B2 * i; a1 = A1 * i; a2 = A2 * i;
    }
};

struct Ir {                     // partitioned impulse-response spectra (2 channels)
    int P = 0;
    std::vector<float> re[2], im[2];
};

struct Voice {
    bool act = false, fast = false;
    int id = -1;
    uint32_t age = 0;
    int nc = 0;                 // carriers
    float base[4], og[4], ph[4];
    int wave[4];
    int mode = 0;               // 0 tone, 1 FM, 2 AM
    bool md = false;
    float mph = 0, minc = 0, mdepth = 0, amBase = 1; int mwave = 0;
    int stage = 0; long cnt = 0, aN = 1, dN = 1, sN = 0, rN = 1;
    bool hold = true;
    float lev = 0, relCoef = 0.999f, rtau = 0.01f;
    long relLeft = 0;
};

class Engine {
public:
    void init(float sampleRate);
    // control (any thread)
    void noteOn(int id, const float* snd /*NSOUND*/, float ratio);
    void noteOff(int id);
    void cut(int id);
    void allOff();
    void setGlobal(const float* g /*NGLOB*/);
    void setDrone(bool on, float base);
    void setReverb(float decay, float damp);     // rebuilds the impulse response (worker thread)
    // audio thread
    void render(float* out, int frames);          // interleaved stereo
    ~Engine();
private:
    struct Ev { int type, id; float ratio; float snd[NSOUND]; };
    static const int RN = 512;
    Ev ring[RN]; std::atomic<uint32_t> rh{0}, rt{0}; std::mutex pm;
    void push(const Ev& e);
    void apply(const Ev& e);
    float sr = 48000; bool inited = false;
    std::atomic<float> G[NGLOB];
    std::atomic<int> droneOn{0}; std::atomic<float> droneBase{130.81f};
    Voice v[NVOICE]; uint32_t ageC = 0;
    // sample tables
    // chain state
    float lfoPh = 0, vibS = 0, trmS = 0, wobS = 0, chS = 0, echoS = 0, revS = 0, preS = 1;
    float cutS = 20000, resS = 0.7f, etS = 0.35f, efbS = 0.35f, lrS = 5;
    float eqS[7] = {0,0,0,0,0,0,0};
    Biquad lp, eq[7];

    // echo
    std::vector<float> dl; int dw = 0; Biquad efl;
    // chorus
    std::vector<float> cd; int cw = 0; float cph = 0;
    // drone
    float dph[4] = {0,0,0,0}; float dg = 0; Biquad dlp;
    // reverb convolver
    static const int CB = 1024, CN = 2048;
    std::vector<float> fftC, fftS;       // twiddles
    std::vector<float> inRe, inIm, accRe, accIm, tmpRe, tmpIm;
    std::vector<float> fdlRe, fdlIm; int fpos = 0; int cfill = 0;
    std::vector<float> inHist; float outB[2][CB]; int outPos = 0;
    Ir* curIr = nullptr; std::atomic<Ir*> newIr{nullptr}; std::atomic<Ir*> retired{nullptr};
    std::vector<int> rev; int pmax = 2;
    bool convRun = false;
    void fftCore(float* re, float* im);
    void convBlock();
    // IR worker
    std::thread wk; std::mutex wm; std::condition_variable wc; bool wreq = false, wquit = false; float wdec = 2, wdmp = 40;
    void worker();
    void ctlUpdate(float lfo);
    void startVoice(const Ev& e);
};

} // namespace sk
