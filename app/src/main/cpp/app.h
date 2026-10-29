#pragma once
#include "engine.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct Preset { char name[16]; OscP o[4]; float a, d, s, r; int mode; };
struct Settings { float kwRel = 1.f / 22.f; float bw = 60, bh = 62, dz = 6; int blk = 0; float pos = -1; };
struct REv { uint32_t t; uint8_t down, key, p0, p1; };
struct Session { char nm[20]; float dur; int n; REv ev[2000]; };
struct Rect { int x = 0, y = 0, w = 0, h = 0; bool in(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; } };
struct Pt { int id; float x, y; };

struct Widget {
    enum Type { Slider, BigRow, Button, Toggle, Radio, Label } type = Label;
    std::string text, unit, text2; float step = 1;
    std::function<float()> mn, mx, get;
    std::function<void(float)> set;
    std::function<void()> click, click2;
    std::function<std::string()> textFn;
    std::function<bool()> isOn;
    std::vector<std::string> opts;
    Rect r, tr, bl, br, r2; std::vector<Rect> orr;
};
struct Panel { std::string title; std::function<bool()> onGet; std::function<void(bool)> onSet; std::vector<Widget> w; bool hasOn = false; };

class App {
public:
    App(Engine& eng, const std::string& dataDir);
    void resize(int w, int h);
    void draw(uint32_t* px, int stride, int w, int h);
    void onTouch(const Pt* p, int n, bool cancel, double nowMs);
    void tick(double nowMs);
    bool dirty() const { return dirty_; }
    bool needTick() const;
    void releaseAll();
    void pubParams();
    void save();
    // for tests
    void openPanel(int i);
    int openedPanel() const { return pn_; }
private:
    enum { K_NONE, K_KEY, K_BAR, K_DRAG, K_REP };
    struct Track { int id = 0, kind = K_NONE, key = -1, bar = -1, wi = -1, dir = 0, n = 0; bool fired = false; float dx = 0; double t0 = 0, next = 0; };
    Engine& eng_; std::string path_;
    Params P; Settings Z; Preset pre_[20]; int sel_ = 0;
    std::vector<Session> ses_;
    int W = 0, H = 0, Lw_ = 0, fs_ = 3, ps_ = 2, bs_ = 2, gap_ = 3, pn_ = -1, pw_ = 0;
    Rect presetR_[20], barR_[14], kbR_, panelR_, closeR_, onR_;
    std::vector<Panel> panels_;
    bool dirty_ = true, rec_ = false, playing_ = false, hold_ = false;
    int cnt_[NKEYS] = {0}; bool pdn_[NKEYS] = {false};
    double now_ = 0, recT0_ = 0, playT0_ = 0; int playSes_ = -1, playIdx_ = 0;
    std::vector<REv> recEv_; std::vector<Track> tracks_; std::string msg_;
    int whiteIdx_[NKEYS]; bool black_[NKEYS]; int whiteKey_[42];
    uint32_t* px_ = nullptr; int stride_ = 0; int cx0_ = 0, cy0_ = 0, cx1_ = 0, cy1_ = 0;
    // drawing
    void clipAll(); void fillRect(int x, int y, int w, int h, uint32_t c); void frame(Rect r, int t, uint32_t c);
    void text(int x, int y, const std::string& s, int sc, uint32_t c); int textW(const std::string& s, int sc) const; int fit(const std::string& s, int maxW, int maxSc) const;
    void button(const Rect& r, const std::string& s, bool on, int sc, bool dim = false);
    void drawKeys(); void drawPanel(); void drawWidget(Widget& w);
    // logic
    void layout(); void layoutPanel(); float kw() const; float maxPos() const;
    int keyAt(float x, float y) const;
    void keyDown(int k, bool user); void keyUp(int k, bool user); void recAdd(int d, int k);
    void applyPreset(int i); void buildPanels(); void buildSessions(); void startRec(); void stopRec(); void startPlay(int s); void stopPlay();
    void startTrack(const Pt& p); void endTrack(Track& t); void panelDown(const Pt& p, Track& t); void dragMove(Track& t, float x);
    void barAction(int b, bool longPress); bool barToggle(int b) const; void closePanel();
    float wmin(const Widget& w) const; float wmax(const Widget& w) const; void repStep(Track& t);
    void load();
};
