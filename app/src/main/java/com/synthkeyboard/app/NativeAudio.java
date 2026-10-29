package com.synthkeyboard.app;

final class NativeAudio {
    static { System.loadLibrary("synthkb"); }
    static native boolean start();
    static native void stop();
    static native void noteOn(int id, float[] snd, float ratio);
    static native void noteOff(int id);
    static native void cut(int id);
    static native void allOff();
    static native void setGlobal(float[] g);
    static native void setDrone(boolean on, float base);
    static native void setReverb(float decay, float damp);
}
