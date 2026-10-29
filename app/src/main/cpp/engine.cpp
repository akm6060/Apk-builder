#include "engine.h"
#include <algorithm>
#include <cmath>
#include <cstring>

const float EQ_FREQ[NEQ] = {128, 256, 512, 1000, 3000, 6000, 9000};
static float g_sine[2050];
static bool g_tab = false;
static const float TWO_PI = 6.28318530718f;

static inline float blep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}
static inline float sineLU(float ph) {
    float x = ph * 2048.f; int i = (int)x; if (i > 2047) i = 2047; if (i < 0) i = 0;
    float f = x - (float)i; return g_sine[i] + (g_sine[i + 1] - g_sine[i]) * f;
}
static inline float oscWave(int w, float ph, float dt) {
    switch (w) {
        case 0: return sineLU(ph);
        case 1: return 1.f - 4.f * fabsf(ph - .5f);
        case 2: return 2.f * ph - 1.f - blep(ph, dt);
        default: { float v = ph < .5f ? 1.f : -1.f; v += blep(ph, dt); float p2 = ph + .5f; if (p2 >= 1.f) p2 -= 1.f; return v - blep(p2, dt); }
    }
}
static void setBq(Biquad& b, float b0, float b1, float b2, float a0, float a1, float a2) {
    float i = 1.f / a0; b.b0 = b0 * i; b.b1 = b1 * i; b.b2 = b2 * i; b.a1 = a1 * i; b.a2 = a2 * i;
}
static void setLP(Biquad& b, float fs, float f, float Q) {
    f = std::min(f, .45f * fs); float w = TWO_PI * f / fs, c = cosf(w), al = sinf(w) / (2.f * Q);
    setBq(b, (1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
}
// type 0 low shelf, 1 peaking, 2 high shelf
static void setEq(Biquad& b, int type, float fs, float f, float db) {
    f = std::min(f, .45f * fs);
    float A = powf(10.f, db / 40.f), w = TWO_PI * f / fs, c = cosf(w), s = sinf(w);
    if (type == 1) {
        float al = s / (2.f * 1.1f);
        setBq(b, 1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    } else {
        float al = s / 2.f * 1.41421356f, be = 2.f * sqrtf(A) * al;
        if (type == 0)
            setBq(b, A * ((A + 1) - (A - 1) * c + be), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - be),
                  (A + 1) + (A - 1) * c + be, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - be);
        else
            setBq(b, A * ((A + 1) + (A - 1) * c + be), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - be),
                  (A + 1) - (A - 1) * c + be, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - be);
    }
}

Engine::Engine() {}

void Engine::init(float sr) {
    sr_ = sr;
    if (!g_tab) { for (int i = 0; i < 2050; i++) g_sine[i] = sinf(TWO_PI * (float)i / 2048.f); g_tab = true; }
    for (auto& v : v_) v = Voice();
    qh_ = 0; qt_ = 0; hold_ = false; age_ = 0; lfoPh_ = 0;
    lp_ = Biquad(); for (auto& e : eq_) e = Biquad();
    eb_.assign((size_t)(sr * 1.2f) + 8, 0.f); ew_ = 0; eLp_ = 0; eDelay_ = eDelayT_ = 800; eIn_ = eInT_ = 0; eWet_ = eWetT_ = 0;
    static const int cl[4] = {1116, 1188, 1277, 1356}, al[2] = {556, 441};
    for (int i = 0; i < 4; i++) { c_[i].b.assign((size_t)(cl[i] * sr / 44100.f) + 1, 0.f); c_[i].i = 0; c_[i].lp = 0; }
    for (int i = 0; i < 2; i++) { ap_[i].assign((size_t)(al[i] * sr / 44100.f) + 1, 0.f); api_[i] = 0; }
    rIn_ = rInT_ = rWet_ = rWetT_ = 0; pre_ = preT_ = 1;
    lastSeq_ = 0xFFFFFFFFu;
}

void Engine::setParams(const Params& p) {
    uint32_t s = seq_.load(std::memory_order_relaxed);
    seq_.store(s + 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    std::memcpy(&shared_, &p, sizeof(Params));
    seq_.store(s + 2, std::memory_order_release);
}

void Engine::push(int t, int k) {
    uint32_t h = qh_.load(std::memory_order_relaxed), n = (h + 1) & 511;
    if (n == qt_.load(std::memory_order_acquire)) return;   // full: drop
    q_[h] = Ev{(uint8_t)t, (uint8_t)k};
    qh_.store(n, std::memory_order_release);
}

void Engine::applyParams() {
    const Params& P = cur_;
    lfoInc_ = P.lfoRate / sr_;
    vibC_ = P.sndOn ? P.vib : 0.f; tremD_ = P.sndOn ? P.trem : 0.f;
    lpOn_ = P.sndOn && P.cut < 99.5f;
    if (lpOn_) setLP(lp_, sr_, 80.f * powf(250.f, P.cut / 100.f), .7f + P.res * .17f);
    bool any = false;
    for (int i = 0; i < NEQ; i++) if (fabsf(P.eq[i]) > .01f) any = true;
    eqOn_ = P.eqOn && any;
    if (eqOn_) for (int i = 0; i < NEQ; i++) setEq(eq_[i], i == 0 ? 0 : (i == NEQ - 1 ? 2 : 1), sr_, EQ_FREQ[i], P.eq[i]);
    eDelayT_ = std::max(8.f, P.echoTime / 1000.f * sr_); eFb_ = std::min(P.echoFb, 85.f) / 100.f;
    eInT_ = (P.fxOn && P.echoOn) ? 1.f : 0.f; eWetT_ = P.fxOn ? P.echoMix / 100.f : 0.f;
    float T = std::max(.1f, P.revDecay);
    for (int i = 0; i < 4; i++) c_[i].g = powf(10.f, -3.f * (float)c_[i].b.size() / (sr_ * T));
    rDamp_ = P.revDamp / 100.f * .85f;
    rInT_ = (P.fxOn && P.reverbOn) ? 1.f : 0.f; rWetT_ = P.fxOn ? P.revMix / 100.f : 0.f;
    preT_ = powf(10.f, P.preamp / 20.f);
}

void Engine::releaseVoice(Voice& v, float tau) {
    if (!v.active || v.rel) return;
    v.rel = true; v.held = false; v.stage = 3;
    v.relCoef = expf(-1.f / (sr_ * std::max(.005f, tau) / 4.f));
}

int Engine::pickVoice() {
    for (int i = 0; i < NVOICES; i++) if (!v_[i].active) return i;
    int best = -1; uint32_t ba = 0xFFFFFFFFu;
    for (int i = 0; i < NVOICES; i++) if (v_[i].rel && v_[i].age < ba) { ba = v_[i].age; best = i; }
    if (best >= 0) return best;
    ba = 0xFFFFFFFFu;
    for (int i = 0; i < NVOICES; i++) if (v_[i].age < ba) { ba = v_[i].age; best = i; }
    return best;
}

void Engine::startVoice(int key) {
    const Params& P = cur_;
    if (!P.poly) { for (auto& v : v_) releaseVoice(v, .012f); }
    else for (auto& v : v_) if (v.active && v.key == key && !v.rel) releaseVoice(v, .012f);
    int slot = pickVoice(); Voice& v = v_[slot]; v = Voice();
    float ratio = exp2f((float)(key + 36 - 60 + P.transpose) / 12.f) * P.tuneA4 / 440.f;
    int idx[4], nc = 0;
    for (int k = 0; k < 4; k++) {
        bool on = P.oscOn[k] && P.o[k].vol > 0 && P.o[k].freq > 0 && !(k == 3 && P.mode > 0);
        if (on) idx[nc++] = k;
    }
    bool mod = P.mode > 0 && P.oscOn[3] && P.o[3].vol > 0 && P.o[3].freq > 0 && nc > 0;
    float nrm = sqrtf((float)std::max(1, nc));
    for (int j = 0; j < nc; j++) {
        const OscP& o = P.o[idx[j]];
        float f = std::min(o.freq * ratio * exp2f((float)o.semi / 12.f) * exp2f((float)o.cents / 1200.f), 18000.f);
        v.w[j] = o.wave; v.dt[j] = std::min(.45f, f / sr_); v.g[j] = o.vol / 100.f * .5f / nrm;
    }
    v.n = nc;
    if (mod) {
        v.mode = P.mode; v.w4 = P.o[3].wave; v.dt4 = std::min(.45f, P.o[3].freq / sr_);
        v.fmD = P.o[3].vol / 100.f * .5f; v.amD = P.o[3].vol / 200.f;
    }
    float A, D, S, R;
    if (P.adsrOn) { A = std::max(P.a, .006f); D = std::max(P.d, .01f); S = P.s; R = std::max(P.r, .02f); }
    else { A = .006f; D = .01f; S = 5.f; R = .05f; }
    v.aInc = 1.f / (A * sr_); v.dDec = .3f / (D * sr_); v.sustHold = S >= 4.99f; v.sustLeft = S;
    v.relCoef = expf(-1.f / (sr_ * R / 4.f));
    v.active = true; v.key = key; v.age = ++age_; v.stage = 0; v.env = 0;
}

void Engine::handle(const Ev& e) {
    switch (e.t) {
        case 0: startVoice(e.k); break;
        case 1:
            for (auto& v : v_) if (v.active && v.key == e.k && !v.rel) {
                if (hold_) v.held = true; else { v.rel = true; v.stage = 3; }
            }
            break;
        case 2: hold_ = true; break;
        case 3: hold_ = false; for (auto& v : v_) if (v.active && v.held) { v.held = false; v.rel = true; v.stage = 3; } break;
        case 4: for (auto& v : v_) { v.held = false; releaseVoice(v, .03f); } break;
    }
}

void Engine::block(float* out, int n) {
    float mix[32]; for (int i = 0; i < n; i++) mix[i] = 0.f;
    float vibF = vibC_ > 0.f ? exp2f(vibC_ * sineLU(lfoPh_) / 1200.f) : 1.f;
    for (auto& v : v_) {
        if (!v.active) continue;
        for (int i = 0; i < n; i++) {
            if (v.stage == 0) { v.env += v.aInc; if (v.env >= 1.f) { v.env = 1.f; v.stage = 1; } }
            else if (v.stage == 1) { v.env -= v.dDec; if (v.env <= .7f) { v.env = .7f; v.stage = 2; } }
            else if (v.stage == 2) { if (!v.sustHold) { v.sustLeft -= 1.f / sr_; if (v.sustLeft <= 0.f) v.stage = 3; } }
            else { v.env *= v.relCoef; if (v.env < 1e-4f) { v.active = false; break; } }
            float m = 0.f;
            if (v.mode) { m = oscWave(v.w4, v.ph4, v.dt4); v.ph4 += v.dt4; if (v.ph4 >= 1.f) v.ph4 -= 1.f; }
            float s = 0.f, fm = v.mode == 1 ? 1.f + v.fmD * m : 1.f;
            for (int j = 0; j < v.n; j++) {
                float dt = v.dt[j] * vibF * fm;
                s += oscWave(v.w[j], v.ph[j], dt) * v.g[j];
                v.ph[j] += dt; if (v.ph[j] >= 1.f) v.ph[j] -= 1.f;
            }
            if (v.mode == 2) s *= (1.f - v.amD) + v.amD * m;
            mix[i] += s * v.env;
        }
    }
    // smooth control values once per block
    eDelay_ += (eDelayT_ - eDelay_) * .05f; eIn_ += (eInT_ - eIn_) * .1f; eWet_ += (eWetT_ - eWet_) * .1f;
    rIn_ += (rInT_ - rIn_) * .1f; rWet_ += (rWetT_ - rWet_) * .1f; pre_ += (preT_ - pre_) * .1f;
    const int EL = (int)eb_.size();
    for (int i = 0; i < n; i++) {
        float x = mix[i] * .3f;
        if (lpOn_) x = lp_.proc(x);
        if (tremD_ > 0.f) { float ph = lfoPh_ + lfoInc_ * (float)i; ph -= floorf(ph); x *= (1.f - tremD_ / 200.f) + tremD_ / 200.f * sineLU(ph); }
        if (eqOn_) for (int b = 0; b < NEQ; b++) x = eq_[b].proc(x);
        // echo
        float rp = (float)ew_ - eDelay_; while (rp < 0) rp += (float)EL;
        int r0 = (int)rp; float fr = rp - (float)r0; int r1 = r0 + 1; if (r0 >= EL) r0 -= EL; if (r1 >= EL) r1 -= EL;
        float dl = eb_[r0] + (eb_[r1] - eb_[r0]) * fr;
        eLp_ += (dl - eLp_) * .37f;
        eb_[ew_] = x * eIn_ + eLp_ * eFb_;
        if (++ew_ >= EL) ew_ = 0;
        float y = x + dl * eWet_;
        // reverb
        float ri = x * rIn_, rs = 0.f;
        for (int c = 0; c < 4; c++) {
            Comb& cb = c_[c]; float o = cb.b[cb.i];
            cb.lp = o * (1.f - rDamp_) + cb.lp * rDamp_;
            cb.b[cb.i] = ri * (1.f - cb.g) + cb.lp * cb.g;
            if (++cb.i >= (int)cb.b.size()) cb.i = 0;
            rs += o;
        }
        rs *= .5f;
        for (int a = 0; a < 2; a++) {
            float bo = ap_[a][api_[a]]; float o = -rs + bo; ap_[a][api_[a]] = rs + bo * .5f; rs = o;
            if (++api_[a] >= (int)ap_[a].size()) api_[a] = 0;
        }
        y += rs * rWet_;
        y *= pre_;
        if (y > 1.f) y = 1.f; else if (y < -1.f) y = -1.f;
        out[2 * i] = y; out[2 * i + 1] = y;
    }
    lfoPh_ += lfoInc_ * (float)n; lfoPh_ -= floorf(lfoPh_);
}

void Engine::render(float* out, int frames) {
#if defined(__aarch64__)
    { uint64_t f; asm volatile("mrs %0, fpcr" : "=r"(f)); f |= (1ull << 24); asm volatile("msr fpcr, %0" :: "r"(f)); }   // flush denormals
#endif
    uint32_t t = qt_.load(std::memory_order_relaxed), h = qh_.load(std::memory_order_acquire);
    while (t != h) { handle(q_[t]); t = (t + 1) & 511; }
    qt_.store(t, std::memory_order_release);
    uint32_t s1 = seq_.load(std::memory_order_acquire);
    if (s1 != lastSeq_ && !(s1 & 1)) {
        Params tmp; std::memcpy(&tmp, &shared_, sizeof(Params));
        std::atomic_thread_fence(std::memory_order_acquire);
        if (seq_.load(std::memory_order_relaxed) == s1) { cur_ = tmp; lastSeq_ = s1; applyParams(); }
    }
    int done = 0;
    while (done < frames) { int nb = std::min(32, frames - done); block(out + 2 * done, nb); done += nb; }
}
