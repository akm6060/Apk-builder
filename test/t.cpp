#include "../app/src/main/cpp/engine.h"
#include <cstdio>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
using namespace sk;
static void snd(float* s, float mode, float a, float d, float sus, float r, int w1, float f1, float v1, int w2=0, float f2=262, float v2=0, float f4=64, float v4=0, int w4=0) {
    s[0]=mode; s[1]=a; s[2]=d; s[3]=sus; s[4]=r;
    float* o=s+5; float vals[4][6]={{1,(float)w1,f1,v1,0,0},{1,(float)w2,f2,v2,0,0},{1,0,262,0,0,0},{1,(float)w4,f4,v4,0,0}};
    for(int k=0;k<4;k++) for(int j=0;j<6;j++) o[k*6+j]=vals[k][j];
}
static void gdef(float* g){ for(int i=0;i<NGLOB;i++) g[i]=0; g[G_CUT]=100; g[G_LR]=5; g[G_ET]=350; g[G_EFB]=35; g[G_PRE]=11; }
int main(){
    Engine e; e.init(48000); float g[NGLOB]; gdef(g); e.setGlobal(g);
    std::vector<float> buf(2*192); int fails=0;
    auto run=[&](int n){ std::vector<float> all; for(int i=0;i<n;i++){ e.render(buf.data(),192); all.insert(all.end(),buf.begin(),buf.end()); } return all; };
    // 1. click test per waveform: largest sample step in first 30 ms vs steady-state
    for(int w=0;w<4;w++){
        float s[NSOUND]; snd(s,0,0.0f,.3f,5,.5f,w,262,100);
        run(20); e.noteOn(60,s,1.f); auto a=run(30);
        float on=0; for(size_t i=2;i<a.size()&&i<2*1440;i+=2) on=std::max(on,fabsf(a[i]-a[i-2]));
        float st=0; for(size_t i=a.size()-2*960;i<a.size();i+=2) st=std::max(st,fabsf(a[i]-a[i-2]));
        printf("wave %d: onset max step %.4f, steady max step %.4f\n",w,on,st); if(on>st*1.15f+0.01f){fails++;puts("  CLICK?");}
        e.noteOff(60); run(60);
    }
    // 2. EQ slider crackle: sustained sine, jump gains like a fast slider drag; compare with steady step
    { float s[NSOUND]; snd(s,0,.01f,.3f,5,.5f,0,262,100); e.noteOn(60,s,1.f); auto w0=run(40);
      float base=0; for(size_t i=w0.size()-2*960;i<w0.size();i+=2) base=std::max(base,fabsf(w0[i]-w0[i-2]));
      float mxd=0; float prev=w0[w0.size()-2]; int cnt=0;
      for(int step=0;step<40;step++){ gdef(g); g[G_EQ0+(step%7)]= (step%2)? -35.f:35.f; e.setGlobal(g);
        auto a=run(3); for(size_t i=0;i<a.size();i+=2){ float d=fabsf(a[i]-prev); prev=a[i]; if(cnt++>2000) mxd=std::max(mxd,d);} }
      printf("EQ jump test: base step %.4f, max step during jumps %.4f\n",base,mxd); if(mxd>base*3.f+0.02f){fails++;puts("  EQ ZIPPER?");}
      gdef(g); e.setGlobal(g); e.noteOff(60); run(300); }
    // 3. all FX on + 16 voices CPU
    { gdef(g); g[G_CH]=60; g[G_ECHO]=.3f; g[G_REV]=.3f; g[G_TRM]=40; g[G_VIB]=20; g[G_WOB]=30; for(int i=0;i<7;i++) g[G_EQ0+i]=(i%2?12:-8); e.setGlobal(g); e.setReverb(2.5f,40);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      for(int k=0;k<16;k++){ float s[NSOUND]; snd(s,k%3,.05f,.3f,5,.5f,k%4,262,70,2,263,70,64,30,0); e.noteOn(48+k,s,powf(2.f,(k-12)/12.f)); }
      auto t0=std::chrono::steady_clock::now(); auto a=run(500); auto t1=std::chrono::steady_clock::now();
      double ms=std::chrono::duration<double,std::milli>(t1-t0).count(); float mx=0; bool fin=true; for(float x:a){ if(!std::isfinite(x)) fin=false; mx=std::max(mx,fabsf(x)); }
      printf("heavy: %.1f ms CPU for 2.0 s audio (%.1f%% of one core), peak %.3f finite=%d\n",ms,ms/2000.*100,mx,fin); if(!fin||mx>1.0001f) fails++;
      e.allOff(); auto b=run(2000); float tail=0; for(size_t i=b.size()-2000;i<b.size();i+=2) tail=std::max(tail,fabsf(b[i])); printf("tail peak at end (should be ~0): %.5f\n",tail); if(tail>0.01f) fails++; }
    // 4. level of a plain sine preset
    { gdef(g); e.setGlobal(g); float s[NSOUND]; snd(s,0,.01f,.3f,5,.5f,0,262,100); e.noteOn(60,s,1.f); auto a=run(100); float mx=0; for(float x:a) mx=std::max(mx,fabsf(x)); printf("pure sine peak with preamp 11 dB: %.3f\n",mx); e.allOff(); run(200);}
    // 5. rapid retrigger of same key and adjacent keys
    { float s[NSOUND]; snd(s,0,.01f,.3f,5,.5f,2,262,100,0,524,50); float mxd=0,prev=0;
      for(int i=0;i<200;i++){ e.noteOn(60+(i%3),s,1.f); auto a=run(2); for(size_t j=0;j<a.size();j+=2){ mxd=std::max(mxd,fabsf(a[j]-prev)); prev=a[j]; } if(i%2) e.noteOff(60+(i%3)); }
      printf("retrigger max step %.4f (saw partials, steep edges expected below 0.8)\n",mxd); if(mxd>0.8f) fails++; e.allOff(); run(300); }
    // 6. slot flood: 300 notes with 5 s releases; every new note must still be audible
    { float s[NSOUND]; snd(s,0,.01f,.3f,5,5.f,0,262,100); int silent=0;
      for(int i=0;i<300;i++){ e.noteOn(36+(i%60),s,1.f); auto a=run(1); float m=0; for(float x:a) m=std::max(m,fabsf(x)); auto b2=run(2); for(float x:b2) m=std::max(m,fabsf(x)); if(m<1e-4f) silent++; if(i%3==2) e.noteOff(36+((i-2)%60)); }
      printf("flood: %d of 300 new notes silent\n",silent); if(silent) fails++; e.allOff(); run(400); }

    // 7. lost-event race: audio thread renders continuously while another thread plays notes; every note-on must start
    { std::atomic<bool> go{true}; float s[NSOUND]; snd(s,0,.01f,.3f,5,.5f,0,262,100); std::vector<float> b2(2*64);
      std::thread rd([&]{ while(go) e.render(b2.data(),64); });
      int lost=0, N=6000;
      for(int n=0;n<N;n++){ int id=40+(n%12); e.noteOn(id,s,1.f,1.f,0.f); bool ok=false;
        for(int w=0;w<4000&&!ok;w++){ if(e.alive(id)) ok=true; else std::this_thread::sleep_for(std::chrono::microseconds(10)); }
        if(!ok) lost++; if(n%2) e.noteOff(id); }
      go=false; rd.join();
      printf("race test: %d of %d note-ons never started\n",lost,N); if(lost) fails++;
      e.allOff(); run(400); }
    // 8. key-position volume (small levels so tanh stays linear)
    { float s[NSOUND]; snd(s,0,.01f,.3f,5,.5f,0,262,100); e.noteOn(60,s,1.f,0.05f,0.f); run(60); auto a=run(30); float m1=0; for(float x:a) m1=std::max(m1,fabsf(x));
      e.noteGain(60,0.10f); run(40); auto b=run(30); float m2=0; for(float x:b) m2=std::max(m2,fabsf(x));
      printf("gain 0.05 peak %.4f -> after change to 0.10 peak %.4f (expect ~2x)\n",m1,m2); if(!(m2>m1*1.8f&&m2<m1*2.2f)) fails++;
      e.allOff(); run(200); }
    // 9. glide: A3 held, then E4 (ratio 1.4983) with 1 s glide; track pitch by zero crossings, check no jump and arrival
    { float s[NSOUND]; snd(s,0,.01f,.3f,5,.5f,0,220,100); e.noteOn(57,s,1.f,1.f,1.0f); run(60);
      float r2=powf(2.f,7.f/12.f); e.noteOn(64,s,r2,1.f,1.0f);
      std::vector<float> all; for(int i=0;i<250;i++){ e.render(buf.data(),192); for(int j=0;j<192;j++) all.push_back(buf[j*2]); }   // 1 s
      auto freqAt=[&](double t0,double t1){ int zc=0; size_t a0=(size_t)(t0*48000),a1=(size_t)(t1*48000); for(size_t i=a0+1;i<a1;i++) if(all[i-1]<0&&all[i]>=0) zc++; return zc/(t1-t0); };
      double fa=freqAt(0.02,0.12),fm=freqAt(0.45,0.55),fe=freqAt(0.9,1.0); run(80); std::vector<float> after; for(int i=0;i<40;i++){ e.render(buf.data(),192); for(int j=0;j<192;j++) after.push_back(buf[j*2]); }
      int zc=0; for(size_t i=1;i<after.size();i++) if(after[i-1]<0&&after[i]>=0) zc++; double ff=zc/(after.size()/48000.0);
      float mxs=0; for(size_t i=1;i<all.size();i++) mxs=std::max(mxs,fabsf(all[i]-all[i-1]));
      printf("glide: start %.1f Hz, middle %.1f Hz, end of 1s %.1f Hz, after %.1f Hz (target %.1f), max sample step %.3f\n",fa,fm,fe,ff,220*r2,mxs);
      if(!(fa>220&&fa<250&&fm>235&&fm<300&&ff>325&&ff<335&&mxs<0.12f)) { fails++; puts("  GLIDE?"); }
      int act=0; for(int k=0;k<2;k++) act+=e.alive(57)?1:0; printf("old key voice taken over (alive57=%d, alive64=%d)\n",e.alive(57),e.alive(64)); if(e.alive(57)||!e.alive(64)) fails++;
      e.allOff(); run(300); }
    puts(fails?"FAILED":"ENGINE TESTS PASSED"); return fails;
}
