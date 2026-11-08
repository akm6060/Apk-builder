package com.synthkeyboard.app;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.webkit.JavascriptInterface;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.webkit.WebMessage;
import android.webkit.WebMessagePort;
import android.os.Handler;
import android.os.HandlerThread;
import android.widget.Toast;

/** Synth Keyboard: the web screen in a full-screen WebView; all sound comes from the native Oboe engine. */
public class MainActivity extends Activity {
    private WebView web;
    private ValueCallback<Uri[]> chooser;
    private long lastBack;
    private WebMessagePort port;
    private HandlerThread ctl;
    private Handler ctlH;

    /** Audio commands from the page arrive here through an asynchronous message channel (never blocks the page). */
    private void onMsg(String s) {
        try {
            String[] p = s.split("\\|", -1);
            switch (p[0]) {
                case "n": NativeAudio.noteOn(Integer.parseInt(p[1]), parse(p[2]), Float.parseFloat(p[3]), Float.parseFloat(p[4]), Float.parseFloat(p[5])); break;
                case "f": NativeAudio.noteOff(Integer.parseInt(p[1])); break;
                case "c": NativeAudio.cut(Integer.parseInt(p[1])); break;
                case "a": NativeAudio.allOff(); break;
                case "h": NativeAudio.hush(); break;
                case "g": NativeAudio.noteGain(Integer.parseInt(p[1]), Float.parseFloat(p[2])); break;
                case "G": NativeAudio.setGlobal(parse(p[1])); break;
                case "d": NativeAudio.setDrone("1".equals(p[1]), Float.parseFloat(p[2])); break;
                case "r": NativeAudio.setReverb(Float.parseFloat(p[1]), Float.parseFloat(p[2])); break;
                case "q": {
                    StringBuilder sb = new StringBuilder();
                    for (String k : p[1].split(",")) { try { int id = Integer.parseInt(k.trim()); if (!NativeAudio.alive(id)) sb.append(sb.length() > 0 ? "," : "").append(id); } catch (Exception e) { } }
                    WebMessagePort pt = port; if (pt != null) pt.postMessage(new WebMessage("q|" + sb));
                    break;
                }
            }
        } catch (Throwable t) { }
    }

    private static float[] parse(String csv) {
        String[] p = csv.split(",");
        float[] f = new float[p.length];
        for (int i = 0; i < p.length; i++) { try { f[i] = Float.parseFloat(p[i]); } catch (Exception e) { f[i] = 0f; } }
        return f;
    }

    /** Bridge used by the web page. */
    private final class Bridge {
        @JavascriptInterface public boolean available() { return NativeAudio.loaded; }
        @JavascriptInterface public void on(int id, String csv, double ratio, double gain, double glide) { NativeAudio.noteOn(id, parse(csv), (float) ratio, (float) gain, (float) glide); }
        @JavascriptInterface public void gain(int id, double g) { NativeAudio.noteGain(id, (float) g); }
        @JavascriptInterface public boolean alive(int id) { return NativeAudio.alive(id); }
        @JavascriptInterface public String dead(String csv) {
            StringBuilder sb = new StringBuilder();
            for (String s : csv.split(",")) { try { int id = Integer.parseInt(s.trim()); if (!NativeAudio.alive(id)) sb.append(sb.length() > 0 ? "," : "").append(id); } catch (Exception e) { } }
            return sb.toString();
        }
        @JavascriptInterface public void hush() { NativeAudio.hush(); }
        @JavascriptInterface public void off(int id) { NativeAudio.noteOff(id); }
        @JavascriptInterface public void cut(int id) { NativeAudio.cut(id); }
        @JavascriptInterface public void alloff() { NativeAudio.allOff(); }
        @JavascriptInterface public void g(String csv) { NativeAudio.setGlobal(parse(csv)); }
        @JavascriptInterface public void drone(boolean on, double base) { NativeAudio.setDrone(on, (float) base); }
        @JavascriptInterface public void rev(double decay, double damp) { NativeAudio.setReverb((float) decay, (float) damp); }
        @JavascriptInterface public void setOutput(int mode) { NativeAudio.setOutput(mode); }
        @JavascriptInterface public String info() { return NativeAudio.loaded ? NativeAudio.info() : "Native audio not available"; }
    }

    @SuppressLint("SetJavaScriptEnabled")
    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        Window w = getWindow();
        w.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON | WindowManager.LayoutParams.FLAG_FULLSCREEN);
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams lp = w.getAttributes();
            lp.layoutInDisplayCutoutMode = Build.VERSION.SDK_INT >= 30
                    ? WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS
                    : WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            w.setAttributes(lp);
        }
        w.setStatusBarColor(Color.TRANSPARENT);
        w.setNavigationBarColor(Color.TRANSPARENT);

        web = new WebView(this);
        if (Build.VERSION.SDK_INT >= 29) web.addOnLayoutChangeListener((v, l, t, r, bt, ol, ot, or2, ob) ->
                v.setSystemGestureExclusionRects(java.util.Collections.singletonList(new android.graphics.Rect(0, 0, v.getWidth(), v.getHeight()))));
        web.setBackgroundColor(Color.parseColor("#1d110c"));
        web.setOverScrollMode(View.OVER_SCROLL_NEVER);
        web.setVerticalScrollBarEnabled(false);
        web.setHorizontalScrollBarEnabled(false);
        web.setHapticFeedbackEnabled(false);
        web.setLongClickable(false);
        web.setOnLongClickListener(v -> true);

        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setDatabaseEnabled(true);
        s.setMediaPlaybackRequiresUserGesture(false);
        s.setTextZoom(100);
        s.setSupportZoom(false);
        s.setBuiltInZoomControls(false);
        s.setDisplayZoomControls(false);

        NativeAudio.start();
        web.addJavascriptInterface(new Bridge(), "NativeSynth");
        web.addJavascriptInterface(new Object() {
            @JavascriptInterface public void share(final String text) {
                runOnUiThread(() -> {
                    Intent i = new Intent(Intent.ACTION_SEND);
                    i.setType("text/plain");
                    i.putExtra(Intent.EXTRA_TEXT, text);
                    startActivity(Intent.createChooser(i, "Export session"));
                });
            }
            @JavascriptInterface public void exit() { runOnUiThread(() -> finishAndRemoveTask()); }
        }, "Android");

        ctl = new HandlerThread("synth-ctl"); ctl.start(); ctlH = new Handler(ctl.getLooper());
        web.setWebViewClient(new WebViewClient() {
            @Override public void onPageFinished(WebView v, String url) {
                try {
                    WebMessagePort[] pp = v.createWebMessageChannel();
                    port = pp[0];
                    port.setWebMessageCallback(new WebMessagePort.WebMessageCallback() {
                        @Override public void onMessage(WebMessagePort pt, WebMessage m) { String d = m.getData(); if (d != null) onMsg(d); }
                    }, ctlH);
                    v.postWebMessage(new WebMessage("skport", new WebMessagePort[]{pp[1]}), Uri.parse("*"));
                } catch (Throwable t) { port = null; }
            }
        });
        web.setWebChromeClient(new WebChromeClient() {
            @Override public boolean onShowFileChooser(WebView v, ValueCallback<Uri[]> cb, FileChooserParams p) {
                if (chooser != null) chooser.onReceiveValue(null);
                chooser = cb;
                try { startActivityForResult(p.createIntent(), 1); }
                catch (Exception e) { chooser = null; return false; }
                return true;
            }
        });

        setVolumeControlStream(AudioManager.STREAM_MUSIC);
        try { if (Build.VERSION.SDK_INT >= 24 && ((android.os.PowerManager) getSystemService(POWER_SERVICE)).isSustainedPerformanceModeSupported()) w.setSustainedPerformanceMode(true); } catch (Exception ignored) {}

        setContentView(web);
        goFullScreen();
        if (b == null) web.loadUrl("file:///android_asset/index.html"); else web.restoreState(b);
    }

    @SuppressWarnings("deprecation")
    private void goFullScreen() {
        Window w = getWindow();
        if (Build.VERSION.SDK_INT >= 30) {
            w.setDecorFitsSystemWindows(false);
            WindowInsetsController c = w.getInsetsController();
            if (c != null) {
                c.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                c.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            w.getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        }
    }

    @Override public void onWindowFocusChanged(boolean f) { super.onWindowFocusChanged(f); if (f) goFullScreen(); }

    @Override protected void onActivityResult(int req, int res, Intent data) {
        super.onActivityResult(req, res, data);
        if (req == 1 && chooser != null) {
            chooser.onReceiveValue(WebChromeClient.FileChooserParams.parseResult(res, data));
            chooser = null;
        }
    }

    @Override protected void onPause() {
        super.onPause();
        if (web != null) { web.evaluateJavascript("try{offAll()}catch(e){}", null); web.onPause(); }
        if (NativeAudio.loaded) NativeAudio.pause();
    }
    @Override protected void onResume() {
        super.onResume();
        if (NativeAudio.loaded) NativeAudio.start();
        if (web != null) web.onResume();
        goFullScreen();
    }
    @Override protected void onSaveInstanceState(Bundle o) { super.onSaveInstanceState(o); if (web != null) web.saveState(o); }

    @SuppressWarnings("deprecation")
    @Override public void onBackPressed() {
        long now = System.currentTimeMillis();
        if (now - lastBack < 2000) { super.onBackPressed(); return; }
        lastBack = now;
        Toast.makeText(this, "Press back again to exit", Toast.LENGTH_SHORT).show();
    }

    @Override protected void onDestroy() {
        if (web != null) { web.loadUrl("about:blank"); web.destroy(); web = null; }
        if (NativeAudio.loaded) NativeAudio.stop();
        super.onDestroy();
    }
}
