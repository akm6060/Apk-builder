#include "../app/src/main/cpp/engine.h"
#include <cstdio>
#include <chrono>
#include <thread>
#include <vector>
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
    puts(fails?"FAILED":"ENGINE TESTS PASSED"); return fails;
}
