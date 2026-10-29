#include "app.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
constexpr uint32_t RGB(int r, int g, int b) { return 0xFF000000u | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r; }
const uint32_t C_BG = RGB(22, 13, 8), C_PN = RGB(43, 26, 15), C_GD = RGB(217, 168, 79), C_TX = RGB(243, 230, 207),
    C_MUT = RGB(180, 154, 120), C_BT = RGB(74, 43, 21), C_BD = RGB(122, 90, 48), C_KB = RGB(13, 8, 5), C_W = RGB(246, 236, 213),
    C_WD = RGB(229, 180, 90), C_B = RGB(26, 26, 26), C_BDN = RGB(168, 99, 15), C_TRK = RGB(21, 13, 8), C_REC = RGB(220, 38, 38),
    C_ON = RGB(217, 168, 79);

// 5x7 font, ASCII 0x20..0x5A, column bytes (bit0 = top row)
const uint8_t FONT[59][5] = {
{0,0,0,0,0},{0,0,0x5F,0,0},{0,7,0,7,0},{0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,8,0x64,0x62},{0x36,0x49,0x56,0x20,0x50},{0,8,7,3,0},
{0,0x1C,0x22,0x41,0},{0,0x41,0x22,0x1C,0},{0x2A,0x1C,0x7F,0x1C,0x2A},{8,8,0x3E,8,8},{0,0x80,0x70,0x30,0},{8,8,8,8,8},{0,0,0x60,0x60,0},{0x20,0x10,8,4,2},
{0x3E,0x51,0x49,0x45,0x3E},{0,0x42,0x7F,0x40,0},{0x72,0x49,0x49,0x49,0x46},{0x21,0x41,0x49,0x4D,0x33},{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x31},{0x41,0x21,0x11,9,7},
{0x36,0x49,0x49,0x49,0x36},{0x46,0x49,0x49,0x29,0x1E},{0,0,0x14,0,0},{0,0x40,0x34,0,0},{0,8,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},{0,0x41,0x22,0x14,8},{2,1,0x59,9,6},
{0x3E,0x41,0x5D,0x59,0x4E},{0x7C,0x12,0x11,0x12,0x7C},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x41,0x3E},{0x7F,0x49,0x49,0x49,0x41},{0x7F,9,9,9,1},{0x3E,0x41,0x41,0x51,0x73},
{0x7F,8,8,8,0x7F},{0,0x41,0x7F,0x41,0},{0x20,0x40,0x41,0x3F,1},{0x7F,8,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},{0x7F,2,0x1C,2,0x7F},{0x7F,4,8,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
{0x7F,9,9,9,6},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,9,0x19,0x29,0x46},{0x26,0x49,0x49,0x49,0x32},{3,1,0x7F,1,3},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
{0x63,0x14,8,0x14,0x63},{3,4,0x78,4,3},{0x61,0x59,0x49,0x4D,0x43}};

struct FP { const char* n; OscP o[4]; float a, d, s, r; int mode; };
const FP FAC[20] = {
{"PURE SINE",{{0,262,100},{0,262,0},{0,262,0},{0,64,0}},.01f,.3f,5,.5f,0},
{"ORGAN",{{0,262,100},{0,524,60},{0,786,40},{0,64,0}},.01f,.1f,5,.3f,0},
{"SOFT PAD",{{1,262,100},{1,264,70},{0,131,60},{0,64,0}},.4f,1,5,2,0},
{"BUZZ SAW",{{2,262,100},{2,265,80},{2,131,50},{0,64,0}},.01f,.3f,5,.4f,0},
{"SQUARE LEAD",{{3,262,80},{3,524,40},{2,263,40},{0,64,0}},.01f,.2f,5,.2f,0},
{"BELL",{{0,262,100},{0,742,50},{0,1309,30},{0,64,0}},.005f,2.5f,0,3,0},
{"SUB BASS",{{0,262,100},{0,131,80},{3,131,30},{0,64,70}},.01f,.4f,5,.3f,0},
{"FLUTE",{{0,262,100},{0,524,20},{1,786,8},{0,64,0}},.08f,.3f,5,.4f,0},
{"STRINGS",{{2,262,60},{2,264,60},{1,523,40},{0,64,0}},.25f,1,5,1.5f,0},
{"CHORUS KEYS",{{1,262,100},{1,266,70},{0,258,70},{0,64,0}},.01f,1.5f,5,1,0},
{"PIANO",{{1,262,100},{0,524,50},{0,786,25},{0,64,0}},.005f,.1f,0,1.6f,0},
{"PIPE ORGAN",{{0,262,100},{0,524,60},{0,131,60},{0,64,0}},.06f,.1f,5,.25f,0},
{"PICCOLO",{{0,1048,100},{0,2096,20},{1,3144,8},{0,64,0}},.03f,.2f,5,.2f,0},
{"FRENCH HORN",{{1,262,100},{2,263,35},{0,131,50},{0,64,0}},.15f,.4f,5,.5f,0},
{"ACCORDION",{{2,262,60},{2,265,60},{3,524,25},{0,6,30}},.05f,.2f,5,.15f,2},
{"GUITAR",{{1,262,100},{2,524,25},{0,786,15},{0,64,0}},.003f,.08f,0,1.2f,0},
{"ATMOSPHERE",{{1,262,70},{0,396,50},{0,524,40},{0,5,4}},.8f,1,5,3,1},
{"SITAR",{{2,262,70},{2,267,40},{3,786,25},{0,64,0}},.005f,.1f,0,1.8f,0},
{"POLYSYNTH",{{2,262,70},{2,263,70},{3,131,40},{0,64,0}},.02f,.3f,5,.5f,0},
{"ROCK ORGAN",{{3,262,70},{2,524,50},{0,1048,30},{0,64,0}},.005f,.05f,5,.1f,0}};

const char* BARL[14] = {"REC", "PLAY", "POLY", "O1", "O2", "O3", "O4", "ADSR", "EQ", "FX", "SND", "HOLD", "SET", "SAVE"};
const int BAR_PANEL[14] = {-1, 10, -1, 0, 1, 2, 3, 4, 5, 6, 7, -1, 8, 9};
struct Hdr { uint32_t magic, ver, szP, szZ, szPre, szSes; };
const uint32_t MAGIC = 0x53594E54u;

Widget mkSlider(const char* t, const char* u, float mn, float mx, float st, std::function<float()> g, std::function<void(float)> s, bool big = false) {
    Widget w; w.type = big ? Widget::BigRow : Widget::Slider; w.text = t; w.unit = u; w.step = st;
    w.mn = [mn] { return mn; }; w.mx = [mx] { return mx; }; w.get = g; w.set = s; return w;
}
Widget mkToggle(const char* t, std::function<bool()> on, std::function<void()> click) { Widget w; w.type = Widget::Toggle; w.text = t; w.isOn = on; w.click = click; return w; }
Widget mkButton(const char* t, std::function<void()> click) { Widget w; w.type = Widget::Button; w.text = t; w.click = click; return w; }
Widget mkRadio(std::vector<std::string> o, std::function<float()> g, std::function<void(float)> s) { Widget w; w.type = Widget::Radio; w.opts = o; w.get = g; w.set = s; return w; }
Widget mkLabel(std::function<std::string()> f) { Widget w; w.type = Widget::Label; w.textFn = f; return w; }
}  // namespace

// ---------------------------------------------------------------- setup
App::App(Engine& eng, const std::string& dataDir) : eng_(eng), path_(dataDir + "/synthkb.dat") {
    int wi = 0;
    for (int k = 0; k < NKEYS; k++) {
        int n = (k + 36) % 12; black_[k] = (n == 1 || n == 3 || n == 6 || n == 8 || n == 10);
        if (black_[k]) whiteIdx_[k] = wi; else { whiteIdx_[k] = wi; whiteKey_[wi] = k; wi++; }
    }
    for (int i = 0; i < 20; i++) {
        std::memset(&pre_[i], 0, sizeof(Preset)); std::snprintf(pre_[i].name, 16, "%s", FAC[i].n);
        for (int j = 0; j < 4; j++) pre_[i].o[j] = FAC[i].o[j];
        pre_[i].a = FAC[i].a; pre_[i].d = FAC[i].d; pre_[i].s = FAC[i].s; pre_[i].r = FAC[i].r; pre_[i].mode = FAC[i].mode;
    }
    applyPreset(0);
    load();
    buildPanels();
}

void App::pubParams() { eng_.setParams(P); }
void App::applyPreset(int i) {
    sel_ = i; const Preset& p = pre_[i];
    for (int j = 0; j < 4; j++) { P.o[j] = p.o[j]; P.oscOn[j] = 1; }
    P.a = p.a; P.d = p.d; P.s = p.s; P.r = p.r; P.mode = p.mode; P.adsrOn = 1;
    if (P.mode == 1) P.o[3].vol = std::max(1.f, std::min(10.f, P.o[3].vol));
    pubParams(); dirty_ = true;
}

void App::load() {
    FILE* f = std::fopen(path_.c_str(), "rb"); if (!f) return;
    Hdr h; Params p; Settings z; Preset pr[20]; int sel = 0, n = 0;
    bool ok = std::fread(&h, sizeof h, 1, f) == 1 && h.magic == MAGIC && h.ver == 1 && h.szP == sizeof(Params) && h.szZ == sizeof(Settings) && h.szPre == sizeof(Preset) && h.szSes == sizeof(Session)
        && std::fread(&p, sizeof p, 1, f) == 1 && std::fread(&z, sizeof z, 1, f) == 1 && std::fread(pr, sizeof(Preset), 20, f) == 20
        && std::fread(&sel, sizeof sel, 1, f) == 1 && std::fread(&n, sizeof n, 1, f) == 1 && n >= 0 && n <= 10;
    if (ok) {
        std::vector<Session> ss;
        for (int i = 0; i < n && ok; i++) { Session s; if (std::fread(&s, sizeof s, 1, f) == 1) ss.push_back(s); else ok = false; }
        if (ok) { P = p; Z = z; std::memcpy(pre_, pr, sizeof pr); sel_ = std::max(0, std::min(19, sel)); ses_ = ss; pubParams(); }
    }
    std::fclose(f);
}
void App::save() {
    FILE* f = std::fopen(path_.c_str(), "wb"); if (!f) return;
    Hdr h{MAGIC, 1, (uint32_t)sizeof(Params), (uint32_t)sizeof(Settings), (uint32_t)sizeof(Preset), (uint32_t)sizeof(Session)};
    int n = (int)ses_.size();
    std::fwrite(&h, sizeof h, 1, f); std::fwrite(&P, sizeof P, 1, f); std::fwrite(&Z, sizeof Z, 1, f); std::fwrite(pre_, sizeof(Preset), 20, f);
    std::fwrite(&sel_, sizeof sel_, 1, f); std::fwrite(&n, sizeof n, 1, f);
    for (auto& s : ses_) std::fwrite(&s, sizeof s, 1, f);
    std::fclose(f);
}

// ---------------------------------------------------------------- drawing primitives
void App::clipAll() { cx0_ = 0; cy0_ = 0; cx1_ = W; cy1_ = H; }
void App::fillRect(int x, int y, int w, int h, uint32_t c) {
    int x0 = std::max(x, cx0_), y0 = std::max(y, cy0_), x1 = std::min(x + w, cx1_), y1 = std::min(y + h, cy1_);
    if (x1 <= x0 || y1 <= y0) return;
    for (int yy = y0; yy < y1; yy++) std::fill_n(px_ + (size_t)yy * stride_ + x0, x1 - x0, c);
}
void App::frame(Rect r, int t, uint32_t c) { fillRect(r.x, r.y, r.w, t, c); fillRect(r.x, r.y + r.h - t, r.w, t, c); fillRect(r.x, r.y, t, r.h, c); fillRect(r.x + r.w - t, r.y, t, r.h, c); }
int App::textW(const std::string& s, int sc) const { return s.empty() ? 0 : (int)s.size() * 6 * sc - sc; }
int App::fit(const std::string& s, int maxW, int maxSc) const { int sc = maxSc; while (sc > 1 && textW(s, sc) > maxW) sc--; return sc; }
void App::text(int x, int y, const std::string& s, int sc, uint32_t c) {
    for (char ch : s) {
        int u = (unsigned char)ch; if (u >= 'a' && u <= 'z') u -= 32; if (u < 32 || u > 90) u = '?';
        const uint8_t* g = FONT[u - 32];
        for (int col = 0; col < 5; col++) for (int row = 0; row < 7; row++) if ((g[col] >> row) & 1) fillRect(x + col * sc, y + row * sc, sc, sc, c);
        x += 6 * sc;
    }
}
void App::button(const Rect& r, const std::string& s, bool on, int sc, bool dim) {
    fillRect(r.x, r.y, r.w, r.h, on ? C_ON : C_BT); frame(r, std::max(1, gap_ / 2), on ? RGB(243, 210, 138) : C_BD);
    int f = fit(s, r.w - 8, sc); int tw = textW(s, f);
    text(r.x + (r.w - tw) / 2, r.y + (r.h - 7 * f) / 2, s, f, on ? RGB(43, 26, 15) : (dim ? C_MUT : C_TX));
}

// ---------------------------------------------------------------- layout
float App::kw() const { return std::max(10.f, Z.kwRel * (float)W); }
float App::maxPos() const { return std::max(0.f, 42.f * kw() - (float)Lw_); }
void App::resize(int w, int h) { W = w; H = h; layout(); dirty_ = true; }
void App::layout() {
    if (!W || !H) return;
    fs_ = std::max(2, (int)std::lround(H / 270.0)); ps_ = std::max(1, fs_ - 1); bs_ = std::max(1, fs_ - 1); gap_ = std::max(2, H / 300);
    pw_ = (int)(W * 0.40f); Lw_ = pn_ >= 0 ? W - pw_ : W;
    int pH = std::max(7 * ps_ + 10, H / 23), bH = std::max(7 * bs_ + 16, H / 13);
    for (int i = 0; i < 20; i++) {
        int row = i / 10, c = i % 10; int bw = (Lw_ - gap_ * 11) / 10;
        presetR_[i] = Rect{gap_ + c * (bw + gap_), gap_ + row * (pH + gap_), bw, pH};
    }
    int by = gap_ + 2 * (pH + gap_), bw = (Lw_ - gap_ * 15) / 14;
    for (int i = 0; i < 14; i++) barR_[i] = Rect{gap_ + i * (bw + gap_), by, bw, bH};
    int ky = by + bH + gap_; kbR_ = Rect{0, ky, Lw_, H - ky - gap_};
    panelR_ = Rect{W - pw_, 0, pw_, H};
    if (Z.pos < 0) Z.pos = 7.f * kw();
    Z.pos = std::max(0.f, std::min(Z.pos, maxPos()));
    layoutPanel();
}
void App::layoutPanel() {
    if (pn_ < 0 || pn_ >= (int)panels_.size()) return;
    Panel& p = panels_[pn_]; int pad = gap_ * 2 + fs_, x = panelR_.x + pad, w = panelR_.w - 2 * pad;
    int th = std::max(7 * fs_ + 14, H / 13);
    int cw = textW("CLOSE", fs_) + 28; closeR_ = Rect{panelR_.x + panelR_.w - pad - cw, gap_ * 2, cw, th};
    int ow = textW("OFF", fs_) + 28; onR_ = Rect{closeR_.x - gap_ * 2 - ow, gap_ * 2, ow, th};
    int top = gap_ * 2 + th + gap_ * 3, avail = H - top - gap_ * 2;
    int labelH = 7 * fs_, trackH = 8 * fs_, btnH = 7 * fs_ + fs_ * 5, sp = gap_ * 3;
    auto hOf = [&](const Widget& wd) { return (wd.type == Widget::Slider || wd.type == Widget::BigRow) ? labelH + gap_ * 2 + trackH + sp : btnH + sp; };
    int total = 0; for (auto& wd : p.w) total += hOf(wd);
    float sc = total > avail ? (float)avail / (float)total : 1.f;
    int y = top;
    for (auto& wd : p.w) {
        int hh = (int)(hOf(wd) * sc); wd.r = Rect{x, y, w, hh};
        if (wd.type == Widget::Slider || wd.type == Widget::BigRow) {
            int ty = y + (int)((labelH + gap_ * 2) * sc), th2 = std::max(8, (int)(trackH * sc));
            if (wd.type == Widget::Slider) wd.tr = Rect{x, ty, w, th2};
            else { int bw2 = th2 * 3 / 2; wd.bl = Rect{x, ty, bw2, th2}; wd.br = Rect{x + w - bw2, ty, bw2, th2}; wd.tr = Rect{x + bw2 + gap_ * 2, ty, w - 2 * bw2 - gap_ * 4, th2}; }
        } else {
            int bh2 = std::max(8, (int)(btnH * sc)); wd.r.h = bh2;
            if (wd.type == Widget::Radio) {
                int n = (int)wd.opts.size(); wd.orr.assign(n, Rect());
                int bw2 = (w - (n - 1) * gap_ * 2) / std::max(1, n);
                for (int i = 0; i < n; i++) wd.orr[i] = Rect{x + i * (bw2 + gap_ * 2), y, bw2, bh2};
            } else if (!wd.text2.empty()) {
                int rw = w * 74 / 100; wd.r2 = Rect{x + rw + gap_ * 2, y, w - rw - gap_ * 2, bh2}; wd.r.w = rw;
            }
        }
        y += hh;
    }
}
float App::wmin(const Widget& w) const { return w.mn ? w.mn() : 0.f; }
float App::wmax(const Widget& w) const { return w.mx ? w.mx() : 1.f; }

// ---------------------------------------------------------------- panels
void App::buildPanels() {
    panels_.clear();
    static const float LO[4] = {60, 60, 60, 0}, HI[4] = {9000, 9000, 9000, 128};
    for (int k = 0; k < 4; k++) {
        Panel p; p.title = "OSCILLATOR " + std::to_string(k + 1); p.hasOn = true;
        p.onGet = [this, k] { return P.oscOn[k] != 0; }; p.onSet = [this, k](bool b) { P.oscOn[k] = b; pubParams(); };
        p.w.push_back(mkRadio({"SINE", "TRI", "SAW", "SQR"}, [this, k] { return (float)P.o[k].wave; }, [this, k](float v) { P.o[k].wave = (int)v; pubParams(); }));
        if (k == 3) p.w.push_back(mkRadio({"TONE", "FM", "AM"}, [this] { return (float)P.mode; }, [this](float v) { P.mode = (int)v; if (P.mode == 1) P.o[3].vol = std::max(1.f, std::min(10.f, P.o[3].vol)); pubParams(); }));
        p.w.push_back(mkSlider("FREQUENCY", " HZ", LO[k], HI[k], 1, [this, k] { return P.o[k].freq; }, [this, k](float v) { P.o[k].freq = v; pubParams(); }, true));
        Widget vol = mkSlider("VOLUME", " %", 0, 100, 1, [this, k] { return P.o[k].vol; }, [this, k](float v) { P.o[k].vol = v; pubParams(); });
        if (k == 3) { vol.mn = [this] { return P.mode == 1 ? 1.f : 0.f; }; vol.mx = [this] { return P.mode == 1 ? 10.f : 100.f; }; }
        p.w.push_back(vol);
        p.w.push_back(mkSlider("TRANSPOSE", " ST", -24, 24, 1, [this, k] { return (float)P.o[k].semi; }, [this, k](float v) { P.o[k].semi = (int)v; pubParams(); }));
        p.w.push_back(mkSlider("FINE TUNE", " CT", -50, 50, 1, [this, k] { return (float)P.o[k].cents; }, [this, k](float v) { P.o[k].cents = (int)v; pubParams(); }));
        panels_.push_back(p);
    }
    { Panel p; p.title = "ADSR ENVELOPE"; p.hasOn = true; p.onGet = [this] { return P.adsrOn != 0; }; p.onSet = [this](bool b) { P.adsrOn = b; pubParams(); };
      p.w.push_back(mkSlider("ATTACK", " S", 0, .5f, .01f, [this] { return P.a; }, [this](float v) { P.a = v; pubParams(); }));
      p.w.push_back(mkSlider("DECAY", " S", 0, 3, .05f, [this] { return P.d; }, [this](float v) { P.d = v; pubParams(); }));
      p.w.push_back(mkSlider("SUSTAIN (5 = HOLD)", " S", 0, 5, .1f, [this] { return P.s; }, [this](float v) { P.s = v; pubParams(); }));
      p.w.push_back(mkSlider("RELEASE", " S", 0, 5, .1f, [this] { return P.r; }, [this](float v) { P.r = v; pubParams(); }));
      panels_.push_back(p); }
    { Panel p; p.title = "GRAPHIC EQ +-35 DB"; p.hasOn = true; p.onGet = [this] { return P.eqOn != 0; }; p.onSet = [this](bool b) { P.eqOn = b; pubParams(); };
      for (int i = 0; i < NEQ; i++) { char t[16]; if (EQ_FREQ[i] >= 1000) std::snprintf(t, 16, "%g KHZ", EQ_FREQ[i] / 1000); else std::snprintf(t, 16, "%g HZ", EQ_FREQ[i]);
        p.w.push_back(mkSlider(t, " DB", -35, 35, 1, [this, i] { return P.eq[i]; }, [this, i](float v) { P.eq[i] = v; pubParams(); })); }
      p.w.push_back(mkButton("RESET FLAT", [this] { for (int i = 0; i < NEQ; i++) P.eq[i] = 0; pubParams(); }));
      panels_.push_back(p); }
    { Panel p; p.title = "ECHO AND REVERB"; p.hasOn = true; p.onGet = [this] { return P.fxOn != 0; }; p.onSet = [this](bool b) { P.fxOn = b; pubParams(); };
      p.w.push_back(mkToggle("ECHO", [this] { return P.echoOn != 0; }, [this] { P.echoOn = !P.echoOn; pubParams(); }));
      p.w.push_back(mkSlider("ECHO TIME", " MS", 0, 500, 10, [this] { return P.echoTime; }, [this](float v) { P.echoTime = v; pubParams(); }));
      p.w.push_back(mkSlider("FEEDBACK", " %", 0, 85, 1, [this] { return P.echoFb; }, [this](float v) { P.echoFb = v; pubParams(); }));
      p.w.push_back(mkSlider("ECHO MIX", " %", 0, 100, 1, [this] { return P.echoMix; }, [this](float v) { P.echoMix = v; pubParams(); }));
      p.w.push_back(mkToggle("REVERB", [this] { return P.reverbOn != 0; }, [this] { P.reverbOn = !P.reverbOn; pubParams(); }));
      p.w.push_back(mkSlider("DECAY", " S", 0, 3, .1f, [this] { return P.revDecay; }, [this](float v) { P.revDecay = v; pubParams(); }));
      p.w.push_back(mkSlider("DAMPING", " %", 0, 100, 1, [this] { return P.revDamp; }, [this](float v) { P.revDamp = v; pubParams(); }));
      p.w.push_back(mkSlider("REVERB MIX", " %", 0, 100, 1, [this] { return P.revMix; }, [this](float v) { P.revMix = v; pubParams(); }));
      panels_.push_back(p); }
    { Panel p; p.title = "SOUND SHAPING"; p.hasOn = true; p.onGet = [this] { return P.sndOn != 0; }; p.onSet = [this](bool b) { P.sndOn = b; pubParams(); };
      p.w.push_back(mkSlider("FILTER CUTOFF", " %", 0, 100, 1, [this] { return P.cut; }, [this](float v) { P.cut = v; pubParams(); }));
      p.w.push_back(mkSlider("RESONANCE", " %", 0, 100, 1, [this] { return P.res; }, [this](float v) { P.res = v; pubParams(); }));
      p.w.push_back(mkSlider("LFO RATE", " HZ", .1f, 12, .1f, [this] { return P.lfoRate; }, [this](float v) { P.lfoRate = v; pubParams(); }));
      p.w.push_back(mkSlider("VIBRATO", " CT", 0, 100, 1, [this] { return P.vib; }, [this](float v) { P.vib = v; pubParams(); }));
      p.w.push_back(mkSlider("TREMOLO", " %", 0, 100, 1, [this] { return P.trem; }, [this](float v) { P.trem = v; pubParams(); }));
      panels_.push_back(p); }
    { Panel p; p.title = "SETTINGS";
      p.w.push_back(mkSlider("KEYBOARD POSITION", " PX", 0, 1, 1, [this] { return Z.pos; }, [this](float v) { Z.pos = std::max(0.f, std::min(v, maxPos())); }, true));
      p.w.back().mx = [this] { return maxPos(); }; p.w.back().step = 1;
      p.w.push_back(mkSlider("KEY WIDTH", " PX", 10, 100, 1, [this] { return kw(); }, [this](float v) { float o = kw(); Z.kwRel = v / (float)W; Z.pos = Z.pos * kw() / o; Z.pos = std::max(0.f, std::min(Z.pos, maxPos())); }, true));
      p.w.back().mn = [this] { return (float)W / 60.f; }; p.w.back().mx = [this] { return (float)W / 8.f; };
      p.w.push_back(mkSlider("BLACK KEY WIDTH", " %", 10, 120, 1, [this] { return Z.bw; }, [this](float v) { Z.bw = v; }));
      p.w.push_back(mkSlider("BLACK KEY HEIGHT", " %", 30, 100, 1, [this] { return Z.bh; }, [this](float v) { Z.bh = v; }));
      p.w.push_back(mkSlider("KEY EDGE DEAD ZONE", " PX", 0, 20, 1, [this] { return Z.dz; }, [this](float v) { Z.dz = v; }));
      p.w.push_back(mkSlider("PREAMP", " DB", -25, 25, 1, [this] { return P.preamp; }, [this](float v) { P.preamp = v; pubParams(); }));
      p.w.push_back(mkSlider("TRANSPOSE", " ST", -12, 12, 1, [this] { return (float)P.transpose; }, [this](float v) { P.transpose = (int)v; pubParams(); }));
      p.w.push_back(mkSlider("TUNING A4", " HZ", 415, 466, 1, [this] { return P.tuneA4; }, [this](float v) { P.tuneA4 = v; pubParams(); }));
      p.w.push_back(mkToggle("BLOCK WHITE KEYS IN BLACK AREA", [this] { return Z.blk != 0; }, [this] { Z.blk = !Z.blk; }));
      panels_.push_back(p); }
    { Panel p; p.title = "SAVE SOUND PRESET";
      p.w.push_back(mkLabel([this] { return "SELECTED PRESET " + std::to_string(sel_ + 1) + ": " + std::string(pre_[sel_].name); }));
      p.w.push_back(mkButton("SAVE CURRENT SOUND TO IT", [this] {
          Preset& q = pre_[sel_]; for (int j = 0; j < 4; j++) q.o[j] = P.o[j];
          q.a = P.a; q.d = P.d; q.s = P.s; q.r = P.r; q.mode = P.mode; save(); msg_ = "SAVED TO PRESET " + std::to_string(sel_ + 1); }));
      p.w.push_back(mkButton("RESET ALL PRESETS TO FACTORY", [this] {
          for (int i = 0; i < 20; i++) { std::memset(&pre_[i], 0, sizeof(Preset)); std::snprintf(pre_[i].name, 16, "%s", FAC[i].n);
              for (int j = 0; j < 4; j++) pre_[i].o[j] = FAC[i].o[j]; pre_[i].a = FAC[i].a; pre_[i].d = FAC[i].d; pre_[i].s = FAC[i].s; pre_[i].r = FAC[i].r; pre_[i].mode = FAC[i].mode; }
          save(); msg_ = "FACTORY PRESETS RESTORED"; }));
      p.w.push_back(mkLabel([this] { return msg_; }));
      panels_.push_back(p); }
    { Panel p; p.title = "RECORDED SESSIONS"; panels_.push_back(p); }
}
void App::buildSessions() {
    Panel& p = panels_[10]; p.w.clear();
    if (ses_.empty()) p.w.push_back(mkLabel([] { return std::string("NO SESSIONS YET. PRESS REC, PLAY KEYS, PRESS REC AGAIN."); }));
    for (int i = 0; i < (int)ses_.size(); i++) {
        char t[48]; std::snprintf(t, 48, "%d. %.1f S  %d NOTES", i + 1, ses_[i].dur, ses_[i].n / 2);
        Widget w = mkButton(t, [this, i] { startPlay(i); closePanel(); });
        w.text2 = "DELETE"; w.click2 = [this, i] { ses_.erase(ses_.begin() + i); save(); buildSessions(); layoutPanel(); };
        p.w.push_back(w);
    }
}
void App::openPanel(int i) {
    if (i < 0) { closePanel(); return; }
    if (i >= (int)panels_.size()) return;
    if (i == 10) buildSessions();
    if (i == 9) msg_.clear();
    pn_ = i; layout(); dirty_ = true;
}
void App::closePanel() { if (pn_ >= 0) { pn_ = -1; layout(); save(); dirty_ = true; } }

// ---------------------------------------------------------------- keys
int App::keyAt(float x, float y) const {
    if (!kbR_.in(x, y)) return -1;
    float k = kw(), bw = k * Z.bw / 100.f, bkh = kbR_.h * Z.bh / 100.f, u = x + Z.pos;
    float dz = std::min(Z.dz, k / 4.f), dzb = std::min(Z.dz, bw / 4.f);
    if (y < kbR_.y + bkh) {
        for (int i = 0; i < NKEYS; i++) if (black_[i]) {
            float c = whiteIdx_[i] * k;
            if (u >= c - bw / 2 && u < c + bw / 2) { float e = std::min(u - (c - bw / 2), (c + bw / 2) - u); return e < dzb ? -1 : i; }
        }
        if (Z.blk) return -1;
    }
    int wi = (int)std::floor(u / k); if (wi < 0 || wi >= 42) return -1;
    float e = std::min(u - wi * k, (wi + 1) * k - u); if (e < dz) return -1;
    return whiteKey_[wi];
}
void App::recAdd(int d, int k) {
    if (!rec_) return;
    REv e{}; e.t = (uint32_t)(now_ - recT0_); e.down = (uint8_t)d; e.key = (uint8_t)k; recEv_.push_back(e);
    if (recEv_.size() >= 2000) stopRec();
}
void App::keyDown(int k, bool user) { cnt_[k]++; if (cnt_[k] == 1) eng_.noteOn(k); if (user) recAdd(1, k); dirty_ = true; }
void App::keyUp(int k, bool user) { if (cnt_[k] > 0) { cnt_[k]--; if (cnt_[k] == 0) eng_.noteOff(k); } if (user) recAdd(0, k); dirty_ = true; }
void App::releaseAll() {
    tracks_.clear(); stopPlay();
    for (int k = 0; k < NKEYS; k++) cnt_[k] = 0;
    eng_.allOff(); hold_ = false; eng_.setHold(false); dirty_ = true;
}
void App::startRec() { recEv_.clear(); rec_ = true; recT0_ = now_; }
void App::stopRec() {
    rec_ = false;
    if (!recEv_.empty()) {
        Session s; std::memset(&s, 0, sizeof s); s.n = (int)recEv_.size(); s.dur = recEv_.back().t / 1000.f;
        std::snprintf(s.nm, 20, "REC"); std::memcpy(s.ev, recEv_.data(), sizeof(REv) * recEv_.size());
        if (ses_.size() >= 10) ses_.erase(ses_.begin());
        ses_.push_back(s); save();
    }
    recEv_.clear(); dirty_ = true;
}
void App::startPlay(int s) { if (s < 0 || s >= (int)ses_.size()) return; stopPlay(); if (rec_) stopRec(); playing_ = true; playSes_ = s; playIdx_ = 0; playT0_ = now_; dirty_ = true; }
void App::stopPlay() {
    playing_ = false;
    for (int k = 0; k < NKEYS; k++) if (pdn_[k]) { pdn_[k] = false; keyUp(k, false); }
    dirty_ = true;
}

// ---------------------------------------------------------------- touch
bool App::barToggle(int b) const { return b >= 3 && b <= 10; }
void App::barAction(int b, bool lng) {
    if (lng) { if (BAR_PANEL[b] >= 0) openPanel(BAR_PANEL[b]); return; }
    switch (b) {
        case 0: if (rec_) stopRec(); else { if (playing_) stopPlay(); startRec(); } break;
        case 1: if (playing_) stopPlay(); else openPanel(10); break;
        case 2: P.poly = !P.poly; pubParams(); break;
        case 3: case 4: case 5: case 6: P.oscOn[b - 3] = !P.oscOn[b - 3]; pubParams(); break;
        case 7: P.adsrOn = !P.adsrOn; pubParams(); break;
        case 8: P.eqOn = !P.eqOn; pubParams(); break;
        case 9: P.fxOn = !P.fxOn; pubParams(); break;
        case 10: P.sndOn = !P.sndOn; pubParams(); break;
        case 11: hold_ = !hold_; eng_.setHold(hold_); break;
        case 12: openPanel(8); break;
        case 13: openPanel(9); break;
    }
    dirty_ = true;
}
void App::repStep(Track& t) {
    if (pn_ < 0 || t.wi < 0 || t.wi >= (int)panels_[pn_].w.size()) return;
    Widget& w = panels_[pn_].w[t.wi]; t.n++;
    int mult = t.n < 14 ? 1 : t.n < 30 ? 5 : t.n < 50 ? 20 : 50;
    float v = w.get() + (float)t.dir * (float)mult * w.step; v = std::max(wmin(w), std::min(wmax(w), v)); w.set(v); dirty_ = true;
}
void App::dragMove(Track& t, float x) {
    if (pn_ < 0 || t.wi < 0 || t.wi >= (int)panels_[pn_].w.size()) return;
    Widget& w = panels_[pn_].w[t.wi]; float hw = (float)w.tr.h, span = (float)w.tr.w - hw; if (span <= 0) return;
    float c = x - t.dx, f = (c - hw / 2 - (float)w.tr.x) / span; f = std::max(0.f, std::min(1.f, f));
    float mn = wmin(w), mx = wmax(w), v = mn + f * (mx - mn);
    if (w.step > 0) v = mn + std::round((v - mn) / w.step) * w.step;
    v = std::max(mn, std::min(mx, v)); w.set(v); dirty_ = true;
}
void App::panelDown(const Pt& p, Track& t) {
    Panel& pn = panels_[pn_]; t.kind = K_NONE;
    if (closeR_.in(p.x, p.y)) { closePanel(); return; }
    if (pn.hasOn && onR_.in(p.x, p.y)) { pn.onSet(!pn.onGet()); dirty_ = true; return; }
    for (int i = 0; i < (int)pn.w.size(); i++) {
        Widget& w = pn.w[i];
        switch (w.type) {
            case Widget::Slider: case Widget::BigRow: {
                float hw = (float)w.tr.h, span = (float)w.tr.w - hw, mn = wmin(w), mx = wmax(w);
                float f = mx > mn ? (w.get() - mn) / (mx - mn) : 0.f; f = std::max(0.f, std::min(1.f, f));
                float hc = (float)w.tr.x + hw / 2 + f * span, pad = (float)gap_ * 3;
                if (p.x >= hc - hw / 2 - pad && p.x <= hc + hw / 2 + pad && p.y >= w.tr.y - pad && p.y <= w.tr.y + w.tr.h + pad) { t.kind = K_DRAG; t.wi = i; t.dx = p.x - hc; return; }
                if (w.type == Widget::BigRow && (w.bl.in(p.x, p.y) || w.br.in(p.x, p.y))) { t.kind = K_REP; t.wi = i; t.dir = w.bl.in(p.x, p.y) ? -1 : 1; t.n = 0; repStep(t); t.n = 0; t.next = now_ + 400; return; }
                break; }
            case Widget::Button: if (w.r.in(p.x, p.y) && w.click) { w.click(); dirty_ = true; return; } if (!w.text2.empty() && w.r2.in(p.x, p.y) && w.click2) { w.click2(); dirty_ = true; return; } break;
            case Widget::Toggle: if (w.r.in(p.x, p.y) && w.click) { w.click(); dirty_ = true; return; } break;
            case Widget::Radio: for (int j = 0; j < (int)w.orr.size(); j++) if (w.orr[j].in(p.x, p.y)) { w.set((float)j); dirty_ = true; return; } break;
            default: break;
        }
    }
}
void App::startTrack(const Pt& p) {
    Track t; t.id = p.id; t.t0 = now_;
    if (pn_ >= 0 && p.x >= Lw_) panelDown(p, t);
    else {
        int hit = -1;
        for (int i = 0; i < 14; i++) { Rect r = barR_[i]; r.y -= gap_; r.h += 2 * gap_; r.x -= gap_ / 2; r.w += gap_; if (r.in(p.x, p.y)) { hit = i; break; } }
        if (hit >= 0) { if (barToggle(hit)) { t.kind = K_BAR; t.bar = hit; } else barAction(hit, false); }
        else {
            int pi = -1;
            for (int i = 0; i < 20; i++) { Rect r = presetR_[i]; r.x -= gap_ / 2; r.w += gap_; r.y -= gap_ / 2; r.h += gap_; if (r.in(p.x, p.y)) { pi = i; break; } }
            if (pi >= 0) applyPreset(pi);
            else { int k = keyAt(p.x, p.y); if (k >= 0) { t.kind = K_KEY; t.key = k; keyDown(k, true); } }
        }
    }
    tracks_.push_back(t); dirty_ = true;
}
void App::endTrack(Track& t) {
    if (t.kind == K_KEY) keyUp(t.key, true);
    else if (t.kind == K_BAR && !t.fired) barAction(t.bar, false);
    dirty_ = true;
}
void App::onTouch(const Pt* p, int n, bool cancel, double nowMs) {
    now_ = nowMs;
    for (size_t i = 0; i < tracks_.size();) {
        bool present = false; if (!cancel) for (int j = 0; j < n; j++) if (p[j].id == tracks_[i].id) present = true;
        if (!present) { Track t = tracks_[i]; tracks_.erase(tracks_.begin() + i); endTrack(t); } else i++;
    }
    for (int j = 0; j < n && !cancel; j++) {
        Track* f = nullptr; for (auto& t : tracks_) if (t.id == p[j].id) f = &t;
        if (!f) startTrack(p[j]); else if (f->kind == K_DRAG) dragMove(*f, p[j].x);
    }
    dirty_ = true;
}
bool App::needTick() const {
    if (playing_) return true;
    for (auto& t : tracks_) if (t.kind == K_REP || (t.kind == K_BAR && !t.fired)) return true;
    return false;
}
void App::tick(double nowMs) {
    now_ = nowMs;
    for (auto& t : tracks_) {
        if (t.kind == K_BAR && !t.fired && now_ - t.t0 >= 450) { t.fired = true; barAction(t.bar, true); break; }
        if (t.kind == K_REP && pn_ >= 0 && now_ >= t.next) { repStep(t); t.next = now_ + 70; }
    }
    if (playing_ && playSes_ >= 0 && playSes_ < (int)ses_.size()) {
        Session& s = ses_[playSes_]; double el = now_ - playT0_;
        while (playIdx_ < s.n && s.ev[playIdx_].t <= el) {
            const REv& e = s.ev[playIdx_++];
            if (e.key < NKEYS) { if (e.down) { if (!pdn_[e.key]) { pdn_[e.key] = true; keyDown(e.key, false); } } else if (pdn_[e.key]) { pdn_[e.key] = false; keyUp(e.key, false); } }
        }
        if (playIdx_ >= s.n && el > (s.n ? s.ev[s.n - 1].t : 0) + 400) stopPlay();
    } else if (playing_) stopPlay();
}

// ---------------------------------------------------------------- drawing
void App::drawKeys() {
    cx0_ = kbR_.x; cy0_ = kbR_.y; cx1_ = kbR_.x + kbR_.w; cy1_ = kbR_.y + kbR_.h;
    fillRect(kbR_.x, kbR_.y, kbR_.w, kbR_.h, C_KB);
    float k = kw(), bw = k * Z.bw / 100.f, bkh = kbR_.h * Z.bh / 100.f;
    for (int wi = 0; wi < 42; wi++) {
        int x = (int)(wi * k - Z.pos); if (x + k < 0 || x > Lw_) continue;
        int kk = whiteKey_[wi]; bool dn = cnt_[kk] > 0;
        fillRect(x + 1, kbR_.y, (int)k - 2, kbR_.h, dn ? C_WD : C_W);
        if ((kk + 36) % 12 == 0) { std::string s = "C" + std::to_string((kk + 36) / 12 - 1); text(x + 6, kbR_.y + kbR_.h - 7 * ps_ - 10, s, ps_, RGB(107, 90, 58)); }
    }
    for (int i = 0; i < NKEYS; i++) if (black_[i]) {
        int x = (int)(whiteIdx_[i] * k - bw / 2 - Z.pos); if (x + bw < 0 || x > Lw_) continue;
        fillRect(x, kbR_.y, (int)bw, (int)bkh, cnt_[i] > 0 ? C_BDN : C_B);
    }
    clipAll();
}
void App::drawWidget(Widget& w) {
    char buf[64];
    switch (w.type) {
        case Widget::Slider: case Widget::BigRow: {
            float v = w.get(), mn = wmin(w), mx = wmax(w);
            if (w.step >= 1) std::snprintf(buf, 64, "%d", (int)std::lround(v)); else if (w.step < .1f) std::snprintf(buf, 64, "%.2f", v); else std::snprintf(buf, 64, "%.1f", v);
            std::string s = w.text + ": " + buf + w.unit; int sc = fit(s, w.r.w, fs_); text(w.r.x, w.r.y, s, sc, C_TX);
            fillRect(w.tr.x, w.tr.y, w.tr.w, w.tr.h, C_TRK); frame(w.tr, 1, C_BD);
            float hw = (float)w.tr.h, f = mx > mn ? (v - mn) / (mx - mn) : 0.f; f = std::max(0.f, std::min(1.f, f));
            int hx = (int)(w.tr.x + f * (w.tr.w - hw));
            fillRect(w.tr.x + 2, w.tr.y + w.tr.h / 2 - 2, std::max(0, hx - w.tr.x), 4, RGB(150, 110, 50));
            fillRect(hx, w.tr.y, (int)hw, w.tr.h, C_GD);
            if (w.type == Widget::BigRow) { button(w.bl, "-", false, fs_ + 1); button(w.br, "+", false, fs_ + 1); }
            break; }
        case Widget::Button: { std::string s = w.textFn ? w.textFn() : w.text; button(w.r, s, false, fs_); if (!w.text2.empty()) button(w.r2, w.text2, false, fs_ - 1 > 0 ? fs_ - 1 : 1); break; }
        case Widget::Toggle: { bool on = w.isOn(); button(w.r, w.text + (on ? ": ON" : ": OFF"), on, fs_); break; }
        case Widget::Radio: { int cur = (int)std::lround(w.get()); for (int i = 0; i < (int)w.opts.size(); i++) button(w.orr[i], w.opts[i], i == cur, fs_); break; }
        case Widget::Label: { std::string s = w.textFn ? w.textFn() : w.text; text(w.r.x, w.r.y + 4, s, fit(s, w.r.w, fs_), C_MUT); break; }
    }
}
void App::drawPanel() {
    if (pn_ < 0) return;
    Panel& p = panels_[pn_];
    fillRect(panelR_.x, 0, panelR_.w, H, C_PN); fillRect(panelR_.x, 0, gap_, H, C_GD);
    int pad = gap_ * 2 + fs_; text(panelR_.x + pad, gap_ * 2 + (closeR_.h - 7 * fs_) / 2, p.title, fit(p.title, onR_.x - panelR_.x - pad * 2, fs_), C_GD);
    if (p.hasOn) { bool on = p.onGet(); button(onR_, on ? "ON" : "OFF", on, fs_); }
    button(closeR_, "CLOSE", false, fs_);
    for (auto& w : p.w) drawWidget(w);
}
void App::draw(uint32_t* px, int stride, int w, int h) {
    px_ = px; stride_ = stride; if (w != W || h != H) { W = w; H = h; layout(); }
    clipAll(); fillRect(0, 0, W, H, C_BG);
    int psc = ps_; for (int i = 0; i < 20; i++) psc = std::min(psc, fit(pre_[i].name, presetR_[i].w - 8, ps_));
    for (int i = 0; i < 20; i++) button(presetR_[i], pre_[i].name, i == sel_, psc);
    for (int i = 0; i < 14; i++) {
        bool on = false; std::string l = BARL[i];
        switch (i) { case 0: on = rec_; l = rec_ ? "STOP" : "REC"; break; case 1: on = playing_; l = playing_ ? "STOP" : "PLAY"; break; case 2: on = !P.poly; l = P.poly ? "POLY" : "MONO"; break;
            case 3: case 4: case 5: case 6: on = P.oscOn[i - 3]; break; case 7: on = P.adsrOn; break; case 8: on = P.eqOn; break; case 9: on = P.fxOn; break; case 10: on = P.sndOn; break; case 11: on = hold_; break; default: break; }
        Rect r = barR_[i];
        if (i == 0 && rec_) { fillRect(r.x, r.y, r.w, r.h, C_REC); frame(r, 2, RGB(255, 150, 150)); int f = fit(l, r.w - 8, bs_); text(r.x + (r.w - textW(l, f)) / 2, r.y + (r.h - 7 * f) / 2, l, f, RGB(255, 255, 255)); }
        else button(r, l, on, bs_);
        if (pn_ >= 0 && BAR_PANEL[i] == pn_) frame(r, 3, RGB(255, 255, 255));
    }
    drawKeys(); drawPanel();
    dirty_ = false;
}
