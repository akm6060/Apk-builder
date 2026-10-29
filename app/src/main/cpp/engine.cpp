#include "engine.h"
#include <algorithm>
#include <cstdlib>

namespace sk {

static const float PI = 3.14159265358979f;
static float SINT[2049];
static inline float clampf(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
static inline float blep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

static inline float waveS(int w, float p, float dt) {
    switch (w) {
        case 0: { float x = p * 2048.f; int i = (int)x; float f = x - i; return SINT[i] + (SINT[i + 1] - SINT[i]) * f; }
        case 1: return p < .25f ? 4 * p : p < .75f ? 2 - 4 * p : 4 * p - 4;
        case 2: { float q = p + .5f; if (q >= 1.f) q -= 1.f; return 2 * q - 1 - blep(q, dt); }
        default: { float q = p + .5f; if (q >= 1.f) q -= 1.f; return (p < .5f ? 1.f : -1.f) + blep(p, dt) - blep(q, dt); }
    }
}
static void flushDenormals() {
#if defined(__aarch64__)
    uint64_t fpcr; asm volatile("mrs %0, fpcr" : "=r"(fpcr)); fpcr |= (1ULL << 24); asm volatile("msr fpcr, %0" :: "r"(fpcr));
#elif defined(__SSE__)
    unsigned s; asm volatile("stmxcsr %0" : "=m"(s)); s |= 0x8040; asm volatile("ldmxcsr %0" :: "m"(s));
#endif
}

// ---- WebAudio-exact biquad coefficient formulas -------------------------------------------
static void lowpassC(Biquad& q, double f, double Qdb, double sr) {
    f = std::min(f, sr * 0.499); double w = 2 * PI * f / sr, c = cos(w), s = sin(w), g = pow(10.0, Qdb / 20.0), al = s / (2 * g);
    q.set((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
}
static void peakC(Biquad& q, double f, double dB, double Q, double sr) {
    double A = pow(10.0, dB / 40.0), w = 2 * PI * f / sr, c = cos(w), al = sin(w) / (2 * Q);
    q.set(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
}
static void lowShelfC(Biquad& q, double f, double dB, double sr) {
    double A = pow(10.0, dB / 40.0), w = 2 * PI * f / sr, c = cos(w), s = sin(w), al = s / 2 * sqrt(2.0), k = 2 * sqrt(A) * al;
    q.set(A * ((A + 1) - (A - 1) * c + k), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - k),
          (A + 1) + (A - 1) * c + k, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - k);
}
static void highShelfC(Biquad& q, double f, double dB, double sr) {
    double A = pow(10.0, dB / 40.0), w = 2 * PI * f / sr, c = cos(w), s = sin(w), al = s / 2 * sqrt(2.0), k = 2 * sqrt(A) * al;
    q.set(A * ((A + 1) + (A - 1) * c + k), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - k),
          (A + 1) - (A - 1) * c + k, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - k);
}
static const float EQF[7] = {128, 256, 512, 1000, 3000, 6000, 9000};

// ---------------------------------------------------------------------------------------------
void Engine::init(float rate) {
    if (inited && rate == sr) { for (auto& x : v) x = Voice(); rt.store(rh.load()); return; }
    inited = true; sr = rate;
    for (int i = 0; i <= 2048; i++) SINT[i] = sinf(2 * PI * i / 2048.f);
    for (int i = 0; i < NGLOB; i++) G[i].store(0);
    G[G_CUT] = 100; G[G_LR] = 5; G[G_ET] = 350; G[G_EFB] = 35; G[G_PRE] = 0;
    for (auto& x : v) x = Voice();
    dl.assign((size_t)(sr * 1.1f) + 8, 0.f); dw = 0;
    cd.assign((size_t)(sr * 0.06f) + 8, 0.f); cw = 0;
    lowpassC(efl, 3500, 1, sr); lowpassC(dlp, 900, 1, sr);
    lp = Biquad(); for (auto& e : eq) e = Biquad();
    cutS = 20000; resS = 0.7f; etS = 0.35f; efbS = 0.35f; lrS = 5; lfoPh = 0; preS = 1; for (auto& e : eqS) e = 0;
    // fft tables
    fftC.resize(CN / 2); fftS.resize(CN / 2);
    for (int i = 0; i < CN / 2; i++) { fftC[i] = cosf(2 * PI * i / CN); fftS[i] = sinf(2 * PI * i / CN); }
    rev.resize(CN); int bits = 11; for (int i = 0; i < CN; i++) { int r = 0; for (int b = 0; b < bits; b++) if (i & (1 << b)) r |= 1 << (bits - 1 - b); rev[i] = r; }
    inRe.assign(CN, 0); inIm.assign(CN, 0); tmpRe.assign(CN, 0); tmpIm.assign(CN, 0); accRe.assign(CB + 1, 0); accIm.assign(CB + 1, 0);
    pmax = (int)ceilf(3.3f * sr / CB) + 2;
    fdlRe.assign((size_t)pmax * (CB + 1), 0); fdlIm.assign((size_t)pmax * (CB + 1), 0);
    inHist.assign(CN, 0); cfill = 0; memset(outB, 0, sizeof outB); outPos = 0; fpos = 0;
    if (!wk.joinable()) wk = std::thread(&Engine::worker, this);
}
Engine::~Engine() {
    { std::lock_guard<std::mutex> l(wm); wquit = true; wreq = true; }
    wc.notify_all(); if (wk.joinable()) wk.join();
    delete curIr; delete newIr.load(); delete retired.load();
}

void Engine::push(const Ev& e) {
    std::lock_guard<std::mutex> l(pm);
    uint32_t h = rh.load(std::memory_order_relaxed), t = rt.load(std::memory_order_acquire);
    if (h - t >= RN) return;
    ring[h % RN] = e; rh.store(h + 1, std::memory_order_release);
}
void Engine::noteOn(int id, const float* s, float ratio) { Ev e; e.type = 1; e.id = id; e.ratio = ratio; memcpy(e.snd, s, sizeof(float) * NSOUND); push(e); }
void Engine::noteOff(int id) { Ev e; memset(&e, 0, sizeof e); e.type = 2; e.id = id; push(e); }
void Engine::cut(int id) { Ev e; memset(&e, 0, sizeof e); e.type = 3; e.id = id; push(e); }
void Engine::allOff() { Ev e; memset(&e, 0, sizeof e); e.type = 4; push(e); }
void Engine::setGlobal(const float* g) { for (int i = 0; i < NGLOB; i++) G[i].store(g[i], std::memory_order_relaxed); }
void Engine::setDrone(bool on, float base) { droneBase.store(base); droneOn.store(on ? 1 : 0); }
void Engine::setReverb(float decay, float damp) {
    { std::lock_guard<std::mutex> l(wm); wdec = decay; wdmp = damp; wreq = true; }
    wc.notify_all();
}

void Engine::startVoice(const Ev& e) {
    const float* s = e.snd;
    // free the same key (web: cut(id) -> 12 ms fade)
    for (auto& q : v) if (q.act && q.id == e.id && !q.fast) { q.fast = true; q.stage = 5; q.rtau = 0.008f; q.relCoef = expf(-1.f / (0.008f * sr)); q.relLeft = (long)(0.12f * sr); }
    int act = 0; for (auto& q : v) if (q.act && !q.fast) act++;
    if (act >= MAXACT) {   // steal: oldest releasing, else oldest
        Voice* c = nullptr;
        for (auto& q : v) if (q.act && !q.fast && q.stage == 5 && (!c || q.age < c->age)) c = &q;
        if (!c) for (auto& q : v) if (q.act && !q.fast && (!c || q.age < c->age)) c = &q;
        if (c) { c->fast = true; c->stage = 5; c->rtau = 0.008f; c->relCoef = expf(-1.f / (0.008f * sr)); c->relLeft = (long)(0.12f * sr); }
    }
    Voice* n = nullptr; for (auto& q : v) if (!q.act) { n = &q; break; }
    if (!n) return;
    Voice& o = *n; o = Voice(); o.act = true; o.id = e.id; o.age = ++ageC;
    float mode = s[0], A = std::max(s[1], 0.010f), D = std::max(s[2], 0.010f), S = s[3], R = std::max(s[4], 0.02f);
    o.aN = std::max(1L, (long)(A * sr)); o.dN = std::max(1L, (long)(D * sr)); o.sN = (long)(S * sr); o.rN = std::max(1L, (long)(R * sr));
    o.hold = S >= 5.f; o.stage = 0; o.cnt = 0; o.lev = 0; o.rtau = std::max(0.04f, R) / 4.f;
    o.relCoef = expf(-1.f / (o.rtau * sr)); o.relLeft = (long)((o.rtau * 4 * 1.6f + 0.05f) * 3.f * sr);
    const float* O = s + 5; // 4 x {on,w,f,v,t,c}
    bool md = mode > 0.5f && O[18 + 3] > 0 && O[18 + 2] > 0 && O[18 + 0] > 0;
    int na = 0; bool use[4];
    for (int k = 0; k < 4; k++) { use[k] = O[k * 6 + 3] > 0 && O[k * 6 + 2] > 0 && O[k * 6] > 0 && !(k == 3 && mode > 0.5f); if (use[k]) na++; }
    float nrm = sqrtf((float)std::max(1, na));
    o.mode = (int)(mode + 0.5f); o.md = md && na > 0;
    for (int k = 0; k < 4; k++) if (use[k]) {
        float f = O[k * 6 + 2] * e.ratio * powf(2.f, O[k * 6 + 4] / 12.f); f = std::min(f, 18000.f);
        f *= powf(2.f, O[k * 6 + 5] / 1200.f);
        o.base[o.nc] = f; o.og[o.nc] = O[k * 6 + 3] / 100.f * 0.5f / nrm; o.wave[o.nc] = (int)O[k * 6 + 1]; o.ph[o.nc] = 0; o.nc++;
    }
    if (o.md) {
        const float* m = O + 18; o.mwave = (int)m[1]; o.minc = m[2] / sr; o.mph = 0;
        if (o.mode == 2) { o.mdepth = m[3] / 200.f; o.amBase = 1.f - m[3] / 200.f; } else o.mdepth = m[3] / 100.f * 0.5f;
    }
}

void Engine::apply(const Ev& e) {
    if (e.type == 1) startVoice(e);
    else if (e.type == 2) {
        for (auto& q : v) if (q.act && q.id == e.id && !q.fast && q.stage != 5) { q.stage = 5; q.relLeft = (long)((q.rtau * 4 * 1.6f + 0.05f) * 3.f * sr); }
    } else if (e.type == 3) {
        for (auto& q : v) if (q.act && q.id == e.id && !q.fast) { q.fast = true; q.stage = 5; q.rtau = 0.008f; q.relCoef = expf(-1.f / (0.008f * sr)); q.relLeft = (long)(0.12f * sr); }
    } else if (e.type == 4) {
        for (auto& q : v) if (q.act && !q.fast && q.stage != 5) { q.stage = 5; q.relLeft = (long)((q.rtau * 4 * 1.6f + 0.05f) * 3.f * sr); }
    }
}

// ---- reverb: uniform partitioned FFT convolution (impulse response identical in character to the web version)
void Engine::fftCore(float* re, float* im) {
    for (int i = 0; i < CN; i++) { int r = rev[i]; if (i < r) { std::swap(re[i], re[r]); std::swap(im[i], im[r]); } }
    for (int len = 2; len <= CN; len <<= 1) {
        int half = len >> 1, step = CN / len;
        for (int i = 0; i < CN; i += len)
            for (int j = 0; j < half; j++) {
                float wr = fftC[j * step], wi = -fftS[j * step];
                float xr = re[i + j + half], xi = im[i + j + half];
                float tr = xr * wr - xi * wi, ti = xr * wi + xi * wr;
                re[i + j + half] = re[i + j] - tr; im[i + j + half] = im[i + j] - ti;
                re[i + j] += tr; im[i + j] += ti;
            }
    }
}
void Engine::worker() {
    flushDenormals();
    for (;;) {
        float dec, dmp;
        { std::unique_lock<std::mutex> l(wm); wc.wait(l, [&] { return wreq; }); if (wquit) return; wreq = false; dec = wdec; dmp = wdmp; }
        delete retired.exchange(nullptr);
        int n = (int)(sr * std::max(0.05f, std::min(dec, 3.2f)));
        std::vector<float> ch[2]; uint32_t rs = 0x9e3779b9u ^ (uint32_t)(dec * 1000);
        float a = 1.f - dmp / 100.f * 0.92f; double sum = 0;
        for (int c = 0; c < 2; c++) {
            ch[c].resize(n); float y = 0;
            for (int i = 0; i < n; i++) {
                rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5;
                float r = ((rs >> 8) * (1.f / 8388608.f)) - 1.f, k = 1.f - (float)i / n;
                y += (r * k * k * k - y) * a; ch[c][i] = y; sum += (double)y * y;
            }
        }
        double rms = sqrt(sum / (2.0 * n)); if (!(rms > 0.000125)) rms = 0.000125;
        float scale = (float)(0.00125 / rms * 44100.0 / sr);
        Ir* ir = new Ir(); ir->P = std::min((n + CB - 1) / CB, pmax - 1);
        std::vector<float> re(CN), im(CN);
        for (int c = 0; c < 2; c++) {
            ir->re[c].assign((size_t)ir->P * (CB + 1), 0); ir->im[c].assign((size_t)ir->P * (CB + 1), 0);
            for (int p = 0; p < ir->P; p++) {
                std::fill(re.begin(), re.end(), 0.f); std::fill(im.begin(), im.end(), 0.f);
                for (int i = 0; i < CB; i++) { int idx = p * CB + i; if (idx < n) re[i] = ch[c][idx] * scale; }
                fftCore(re.data(), im.data());
                memcpy(&ir->re[c][(size_t)p * (CB + 1)], re.data(), (CB + 1) * sizeof(float));
                memcpy(&ir->im[c][(size_t)p * (CB + 1)], im.data(), (CB + 1) * sizeof(float));
            }
        }
        Ir* old = newIr.exchange(ir); delete old;
    }
}
void Engine::convBlock() {
    Ir* n = newIr.exchange(nullptr);
    if (n) { Ir* old = curIr; curIr = n; retired.store(old); std::fill(fdlRe.begin(), fdlRe.end(), 0.f); std::fill(fdlIm.begin(), fdlIm.end(), 0.f); }
    if (!curIr) { memset(outB, 0, sizeof outB); return; }
    memcpy(inRe.data(), inHist.data(), CN * sizeof(float)); std::fill(inIm.begin(), inIm.end(), 0.f);
    fftCore(inRe.data(), inIm.data());
    memcpy(&fdlRe[(size_t)fpos * (CB + 1)], inRe.data(), (CB + 1) * sizeof(float));
    memcpy(&fdlIm[(size_t)fpos * (CB + 1)], inIm.data(), (CB + 1) * sizeof(float));
    for (int c = 0; c < 2; c++) {
        std::fill(accRe.begin(), accRe.end(), 0.f); std::fill(accIm.begin(), accIm.end(), 0.f);
        for (int p = 0; p < curIr->P; p++) {
            int slot = (fpos - p + pmax) % pmax;
            const float *xr = &fdlRe[(size_t)slot * (CB + 1)], *xi = &fdlIm[(size_t)slot * (CB + 1)];
            const float *hr = &curIr->re[c][(size_t)p * (CB + 1)], *hi = &curIr->im[c][(size_t)p * (CB + 1)];
            float *ar = accRe.data(), *ai = accIm.data();
            for (int k = 0; k <= CB; k++) { ar[k] += xr[k] * hr[k] - xi[k] * hi[k]; ai[k] += xr[k] * hi[k] + xi[k] * hr[k]; }
        }
        for (int k = 0; k <= CB; k++) { tmpRe[k] = accRe[k]; tmpIm[k] = -accIm[k]; }   // conj for inverse-by-forward
        for (int k = CB + 1; k < CN; k++) { tmpRe[k] = accRe[CN - k]; tmpIm[k] = accIm[CN - k]; }
        fftCore(tmpRe.data(), tmpIm.data());
        for (int i = 0; i < CB; i++) outB[c][i] = tmpRe[CB + i] * (1.f / CN);
    }
    fpos = (fpos + 1) % pmax;
}

void Engine::ctlUpdate(float lfo) {
    const float al = 1.f - expf(-32.f / (sr * 0.03f));
    float cutT = 80.f * powf(250.f, G[G_CUT].load() / 100.f), resT = 0.7f + G[G_RES].load() * 0.17f;
    cutS += (cutT - cutS) * al; resS += (resT - resS) * al;
    lrS += (G[G_LR].load() - lrS) * al; etS += (G[G_ET].load() / 1000.f - etS) * al; efbS += (G[G_EFB].load() / 100.f - efbS) * al;
    wobS += (G[G_WOB].load() * 24.f - wobS) * al;
    float fc = clampf(cutS * powf(2.f, wobS * lfo / 1200.f), 20.f, 20000.f);
    lowpassC(lp, fc, resS, sr);
    for (int i = 0; i < 7; i++) {
        eqS[i] += (G[G_EQ0 + i].load() - eqS[i]) * al;
        float g = eqS[i];
        if (i == 0) lowShelfC(eq[i], EQF[i], g, sr); else if (i == 6) highShelfC(eq[i], EQF[i], g, sr); else peakC(eq[i], EQF[i], g, 1.1, sr);
    }
}

void Engine::render(float* out, int frames) {
    static thread_local bool fd = false; if (!fd) { flushDenormals(); fd = true; }
    // events
    for (uint32_t t = rt.load(std::memory_order_relaxed), h = rh.load(std::memory_order_acquire); t != h; t++) apply(ring[t % RN]);
    rt.store(rh.load(std::memory_order_acquire), std::memory_order_release);

    const float alS = 1.f - expf(-1.f / (sr * 0.03f));
    const int CH = 32;
    float vm[CH], vib[CH], lf[CH];
    int done = 0;
    while (done < frames) {
        int nF = std::min(CH, frames - done);
        ctlUpdate(SINT[(int)(lfoPh * 2048.f) & 2047]);
        float vibT = G[G_VIB].load();
        for (int i = 0; i < nF; i++) {
            lfoPh += lrS / sr; if (lfoPh >= 1.f) lfoPh -= 1.f;
            float p = lfoPh * 2048.f; int ip = (int)p; float fr = p - ip; lf[i] = SINT[ip] + (SINT[ip + 1] - SINT[ip]) * fr;
            vibS += (vibT - vibS) * alS;
            vib[i] = vibS > 0.001f ? exp2f(vibS * lf[i] / 1200.f) : 1.f; vm[i] = 0;
        }
        for (auto& q : v) {
            if (!q.act) continue;
            for (int i = 0; i < nF; i++) {
                // envelope
                float env;
                switch (q.stage) {
                    case 0: { float x = (float)q.cnt / q.aN; env = x * x * (3 - 2 * x); if (++q.cnt >= q.aN) { q.stage = 1; q.cnt = 0; } break; }
                    case 1: { env = 1.f - 0.3f * (float)q.cnt / q.dN; if (++q.cnt >= q.dN) { q.stage = q.hold ? 2 : 3; q.cnt = 0; } break; }
                    case 2: env = 0.7f; break;
                    case 3: env = 0.7f; if (++q.cnt >= q.sN) { q.stage = 4; q.cnt = 0; } break;
                    case 4: env = 0.7f * (1.f - (float)q.cnt / q.rN); if (++q.cnt >= q.rN) { q.act = false; env = 0; } break;
                    default: q.lev *= q.relCoef; env = q.lev; if (q.lev < 2e-4f || --q.relLeft <= 0) { q.act = false; env = 0; } break;
                }
                if (q.stage != 5) q.lev = env;
                if (!q.act) break;
                // modulator
                float mod = 0;
                if (q.md) {
                    float mdt = q.minc; q.mph += mdt; if (q.mph >= 1.f) q.mph -= 1.f;
                    mod = waveS(q.mwave, q.mph, mdt);
                }
                float s = 0, fm = (q.md && q.mode == 1) ? 1.f + q.mdepth * mod : 1.f;
                for (int k = 0; k < q.nc; k++) {
                    float dt = std::min(0.49f, q.base[k] / sr * vib[i] * fm); if (dt < 0) dt = 0;
                    q.ph[k] += dt; if (q.ph[k] >= 1.f) q.ph[k] -= 1.f;
                    s += q.og[k] * waveS(q.wave[k], q.ph[k], dt);
                }
                if (q.md && q.mode == 2) s *= q.amBase + q.mdepth * mod;
                vm[i] += s * env;
            }
        }
        // chain
        const float dg_t = droneOn.load() ? 0.1f : 0.f; const float dal = 1.f - expf(-1.f / (sr * (droneOn.load() ? 0.4f : 0.15f)));
        const float b = droneBase.load();
        const float incs[4] = {b / sr, b * 1.5f / sr, b * 2.f / sr, b * 0.5f / sr};
        const bool dr = dg_t > 0 || dg > 1e-5f;
        const float trmT = G[G_TRM].load() / 200.f, chT = G[G_CH].load() / 100.f, ecT = G[G_ECHO].load(), rvT = G[G_REV].load(), prT = powf(10.f, G[G_PRE].load() / 20.f);
        const bool revOn = (rvT > 0 || revS > 1e-5f) && (curIr || newIr.load(std::memory_order_acquire));
        for (int i = 0; i < nF; i++) {
            float x = vm[i];
            if (dr) {
                dg += (dg_t - dg) * dal;
                float d = 0;
                for (int k = 0; k < 4; k++) { dph[k] += incs[k]; if (dph[k] >= 1.f) dph[k] -= 1.f; }
                d += 2.f * (dph[0] < 0.5f ? dph[0] : dph[0] - 1.f) * 1.f;    // saw (0 start)
                d += 2.f * (dph[1] < 0.5f ? dph[1] : dph[1] - 1.f);
                float t3 = dph[2] < .25f ? 4 * dph[2] : dph[2] < .75f ? 2 - 4 * dph[2] : 4 * dph[2] - 4; d += t3;
                float p4 = dph[3] * 2048.f; d += SINT[(int)p4 & 2047];
                x += dlp.run(d) * dg;
            }
            x *= 0.3f;
            x = lp.run(x);
            trmS += (trmT - trmS) * alS; x *= (1.f - trmS) + trmS * lf[i];
            for (auto& e : eq) x = e.run(x);
            echoS += (ecT - echoS) * alS; revS += (rvT - revS) * alS; chS += (chT - chS) * alS; preS += (prT - preS) * alS;
            float L = x, R = x;
            // echo
            {
                float dsm = etS * sr; float rp = (float)dw - dsm; while (rp < 0) rp += (float)dl.size();
                int i0 = (int)rp; float fr = rp - i0; int i1 = i0 + 1; if (i1 >= (int)dl.size()) i1 = 0;
                float w = dl[i0] + (dl[i1] - dl[i0]) * fr;
                dl[dw] = echoS * x + efbS * efl.run(w); if (++dw >= (int)dl.size()) dw = 0;
                L += w; R += w;
            }
            // chorus
            if (chS > 1e-4f || chT > 0) {
                cph += 0.6f / sr; if (cph >= 1.f) cph -= 1.f;
                float m = SINT[(int)(cph * 2048.f) & 2047];
                cd[cw] = chS * x; 
                float tl = (0.014f + 0.003f * m) * sr, tr_ = (0.019f - 0.003f * m) * sr;
                float r1 = (float)cw - tl; if (r1 < 0) r1 += (float)cd.size(); float r2 = (float)cw - tr_; if (r2 < 0) r2 += (float)cd.size();
                int a0 = (int)r1, a1 = (a0 + 1) % (int)cd.size(); float fa = r1 - a0; int b0 = (int)r2, b1 = (b0 + 1) % (int)cd.size(); float fb = r2 - b0;
                L += cd[a0] + (cd[a1] - cd[a0]) * fa; R += cd[b0] + (cd[b1] - cd[b0]) * fb;
                if (++cw >= (int)cd.size()) cw = 0;
            }
            // reverb
            if (revOn) {
                convRun = true;
                L += outB[0][cfill]; R += outB[1][cfill];
                inHist[CB + cfill] = x * revS;
                if (++cfill >= CB) { convBlock(); cfill = 0; memcpy(inHist.data(), inHist.data() + CB, CB * sizeof(float)); }
            } else if (convRun) { convRun = false; cfill = 0; std::fill(inHist.begin(), inHist.end(), 0.f); memset(outB, 0, sizeof outB);
                std::fill(fdlRe.begin(), fdlRe.end(), 0.f); std::fill(fdlIm.begin(), fdlIm.end(), 0.f); }
            out[(done + i) * 2] = tanhf(L * preS);
            out[(done + i) * 2 + 1] = tanhf(R * preS);
        }
        done += nF;
    }
}

} // namespace sk
