package com.synthkeyboard.app;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
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

public class MainActivity extends Activity {
    private WebView web;
    private ValueCallback<Uri[]> chooser;

    private static float[] parse(String csv) {
        String[] p = csv.split(",");
        float[] f = new float[p.length];
        for (int i = 0; i < p.length; i++) { try { f[i] = Float.parseFloat(p[i]); } catch (Exception e) { f[i] = 0f; } }
        return f;
    }

    /** Bridge used by the web page: all sound is made by the native low-latency engine. */
    private final class Bridge {
        @JavascriptInterface public void on(int id, String csv, double ratio) { NativeAudio.noteOn(id, parse(csv), (float) ratio); }
        @JavascriptInterface public void off(int id) { NativeAudio.noteOff(id); }
        @JavascriptInterface public void cut(int id) { NativeAudio.cut(id); }
        @JavascriptInterface public void alloff() { NativeAudio.allOff(); }
        @JavascriptInterface public void g(String csv) { NativeAudio.setGlobal(parse(csv)); }
        @JavascriptInterface public void drone(boolean on, double base) { NativeAudio.setDrone(on, (float) base); }
        @JavascriptInterface public void rev(double decay, double damp) { NativeAudio.setReverb((float) decay, (float) damp); }
    }

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        Window w = getWindow();
        w.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        WindowManager.LayoutParams lp = w.getAttributes();
        if (Build.VERSION.SDK_INT >= 30) lp.layoutInDisplayCutoutMode = 3;
        else lp.layoutInDisplayCutoutMode = 1;
        w.setAttributes(lp);
        if (Build.VERSION.SDK_INT >= 30) w.setDecorFitsSystemWindows(false);

        web = new WebView(this);
        web.setLayerType(View.LAYER_TYPE_HARDWARE, null);
        web.setOverScrollMode(View.OVER_SCROLL_NEVER);
        web.setLongClickable(false);
        web.setHapticFeedbackEnabled(false);
        web.setBackgroundColor(0xFF160D08);
        setContentView(web);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setBuiltInZoomControls(false);
        s.setSupportZoom(false);
        web.setWebViewClient(new WebViewClient());
        web.setWebChromeClient(new WebChromeClient() {
            @Override public boolean onShowFileChooser(WebView v, ValueCallback<Uri[]> cb, FileChooserParams p) {
                if (chooser != null) chooser.onReceiveValue(null);
                chooser = cb;
                try { startActivityForResult(p.createIntent(), 1); }
                catch (Exception e) { chooser = null; return false; }
                return true;
            }
        });
        web.addJavascriptInterface(new Bridge(), "NativeSynth");
        web.addJavascriptInterface(new Object() {
            @JavascriptInterface public void share(final String text) {
                runOnUiThread(new Runnable() { public void run() {
                    Intent i = new Intent(Intent.ACTION_SEND);
                    i.setType("text/plain");
                    i.putExtra(Intent.EXTRA_TEXT, text);
                    startActivity(Intent.createChooser(i, "Export session"));
                }});
            }
            @JavascriptInterface public void exit() {
                runOnUiThread(new Runnable() { public void run() { finishAndRemoveTask(); }});
            }
        }, "Android");
        web.loadUrl("file:///android_asset/index.html");
    }

    private void hideBars() {
        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController c = getWindow().getInsetsController();
            if (c != null) {
                c.hide(WindowInsets.Type.systemBars());
                c.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        }
    }

    @Override protected void onActivityResult(int req, int res, Intent data) {
        if (req == 1 && chooser != null) {
            chooser.onReceiveValue(WebChromeClient.FileChooserParams.parseResult(res, data));
            chooser = null;
        }
    }

    @Override protected void onResume() { super.onResume(); NativeAudio.start(); web.onResume(); hideBars(); }
    @Override protected void onPause() {
        NativeAudio.allOff();
        web.evaluateJavascript("try{releaseAll()}catch(e){}", null);
        web.onPause();
        NativeAudio.stop();
        super.onPause();
    }
    @Override public void onWindowFocusChanged(boolean f) { super.onWindowFocusChanged(f); if (f) hideBars(); }
    @Override protected void onDestroy() { web.destroy(); super.onDestroy(); }
}
