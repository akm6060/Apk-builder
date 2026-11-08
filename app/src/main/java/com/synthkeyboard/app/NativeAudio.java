package com.synthkeyboard.app;

final class NativeAudio {
    static boolean loaded;
    static { try { System.loadLibrary("synthkb"); loaded = true; } catch (Throwable t) { loaded = false; } }
    static native boolean start();
    static native void stop();
    static native void pause();
    static native boolean setOutput(int mode);
    static native void noteOn(int id, float[] snd, float ratio, float gain, float glide);
    static native void noteGain(int id, float g);
    static native boolean alive(int id);
    static native void noteOff(int id);
    static native void cut(int id);
    static native void allOff();
    static native void setGlobal(float[] g);
    static native void setDrone(boolean on, float base);
    static native void setReverb(float decay, float damp);
    static native String info();
}
