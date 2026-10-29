#pragma once
#include <atomic>
#include <cstdint>
#include <vector>

constexpr int NKEYS = 72, NVOICES = 16, NEQ = 7;
extern const float EQ_FREQ[NEQ];

struct OscP { int wave = 0; float freq = 262; float vol = 0; int semi = 0; int cents = 0; };

struct Params {
    OscP o[4];
    int oscOn[4] = {1, 1, 1, 1};
    float a = .01f, d = .5f, s = 5.f, r = .8f;
    int adsrOn = 1;
    int mode = 0;                       // oscillator 4: 0 tone, 1 FM, 2 AM
    float eq[NEQ] = {0, 0, 0, 0, 0, 0, 0};
    int eqOn = 1;
    float cut = 100, res = 0, lfoRate = 5, vib = 0, trem = 0;
    int sndOn = 1;
    int echoOn = 0, reverbOn = 0;
    float echoTime = 350, echoFb = 35, echoMix = 30, revDecay = 2, revDamp = 40, revMix = 30;
    int fxOn = 1;
    float preamp = 6, tuneA4 = 440;
    int transpose = 0;
    int poly = 1;
};

struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline float proc(float x) { float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

struct Voice {
    bool active = false, held = false, rel = false;
    int key = -1; uint32_t age = 0;
    int n = 0, w[4] = {0, 0, 0, 0};
    float ph[4] = {0, 0, 0, 0}, dt[4] = {0, 0, 0, 0}, g[4] = {0, 0, 0, 0};
    int mode = 0, w4 = 0; float fmD = 0, amD = 0, ph4 = 0, dt4 = 0;
    int stage = 0; float env = 0, aInc = 0, dDec = 0, sustLeft = 0, relCoef = 0; bool sustHold = true;
};

class Engine {
public:
    Engine();
    void init(float sampleRate);                 // call only while audio is stopped
    void setParams(const Params& p);             // UI thread
    void noteOn(int key)  { push(0, key); }
    void noteOff(int key) { push(1, key); }
    void setHold(bool h)  { push(h ? 2 : 3, 0); }
    void allOff()         { push(4, 0); }
    void render(float* out, int frames);         // audio thread: interleaved stereo
private:
    struct Ev { uint8_t t, k; };
    Ev q_[512]; std::atomic<uint32_t> qh_{0}, qt_{0};
    void push(int t, int k);
    std::atomic<uint32_t> seq_{0}; Params shared_; uint32_t lastSeq_ = 0xFFFFFFFFu; Params cur_;
    float sr_ = 48000;
    Voice v_[NVOICES]; uint32_t age_ = 0; bool hold_ = false;
    void handle(const Ev& e); void applyParams();
    void startVoice(int key); void releaseVoice(Voice& v, float tau); int pickVoice();
    void block(float* out, int n);
    // shaping
    float lfoPh_ = 0, lfoInc_ = 0, vibC_ = 0, tremD_ = 0; bool lpOn_ = false, eqOn_ = false;
    Biquad lp_, eq_[NEQ];
    // echo
    std::vector<float> eb_; int ew_ = 0; float eDelayT_ = 0, eDelay_ = 0, eFb_ = 0, eLp_ = 0, eInT_ = 0, eIn_ = 0, eWetT_ = 0, eWet_ = 0;
    // reverb
    struct Comb { std::vector<float> b; int i = 0; float lp = 0, g = .8f; };
    Comb c_[4]; std::vector<float> ap_[2]; int api_[2] = {0, 0}; float rDamp_ = .3f, rInT_ = 0, rIn_ = 0, rWetT_ = 0, rWet_ = 0;
    float preT_ = 1, pre_ = 1;
};
