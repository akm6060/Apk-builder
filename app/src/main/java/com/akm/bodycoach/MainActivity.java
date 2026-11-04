package com.akm.bodycoach;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.speech.RecognitionListener;
import android.speech.RecognizerIntent;
import android.speech.SpeechRecognizer;
import android.speech.tts.TextToSpeech;
import android.speech.tts.UtteranceProgressListener;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import androidx.webkit.WebViewAssetLoader;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Locale;

public class MainActivity extends Activity {
    private WebView web;
    private WebViewAssetLoader assetLoader;
    private final Handler ui = new Handler(Looper.getMainLooper());

    private SpeechRecognizer recognizer;
    private boolean onDeviceTried = false, useOnDevice = false;
    private boolean wantListen = false, recognizing = false, ttsBusy = false;
    private String listenLang = "en-IN";
    private TextToSpeech tts;
    private boolean ttsReady = false;
    private static final int REQ_MIC = 7;

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        Window w = getWindow();
        w.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        web = new WebView(this);
        setContentView(web);
        hideSystemUi();

        assetLoader = new WebViewAssetLoader.Builder()
                .addPathHandler("/assets/", new WebViewAssetLoader.AssetsPathHandler(this))
                .build();
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setMediaPlaybackRequiresUserGesture(false);
        s.setAllowFileAccess(false);
        s.setCacheMode(WebSettings.LOAD_NO_CACHE);
        web.setLayerType(View.LAYER_TYPE_HARDWARE, null);
        web.setBackgroundColor(0xff0f141c);
        web.setWebChromeClient(new WebChromeClient() {
            @Override
            public boolean onConsoleMessage(android.webkit.ConsoleMessage m) {
                if (m.messageLevel() == android.webkit.ConsoleMessage.MessageLevel.ERROR) android.util.Log.e("BodyCoach", m.message() + " @" + m.sourceId() + ":" + m.lineNumber());
                return true;
            }
        });
        web.setWebViewClient(new WebViewClient() {
            @Override
            public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {
                WebResourceResponse r = assetLoader.shouldInterceptRequest(request.getUrl());
                if (r != null) {
                    String p = request.getUrl().getPath();
                    if (p != null && p.endsWith(".js")) { r.setMimeType("text/javascript"); r.setEncoding("utf-8"); }
                    else if (p != null && p.endsWith(".html")) { r.setMimeType("text/html"); r.setEncoding("utf-8"); }
                    else if (p != null && p.endsWith(".glb")) r.setMimeType("model/gltf-binary");
                }
                return r;
            }
        });
        web.addJavascriptInterface(new Bridge(), "AndroidBridge");
        web.loadUrl("https://appassets.androidplatform.net/assets/index.html");

        tts = new TextToSpeech(this, status -> {
            if (status == TextToSpeech.SUCCESS) {
                ttsReady = true;
                tts.setLanguage(new Locale("en", "IN"));
                tts.setSpeechRate(1.0f);
                tts.setOnUtteranceProgressListener(new UtteranceProgressListener() {
                    @Override public void onStart(String id) { }
                    @Override public void onDone(String id) { if (id.startsWith("gT")) js("window.onSpeakDone && window.onSpeakDone('" + id.substring(1) + "')"); if (id.startsWith("g")) ui.post(() -> { ttsBusy = false; restartSoon(250); }); }
                    @Override public void onError(String id) { if (id.startsWith("gT")) js("window.onSpeakDone && window.onSpeakDone('" + id.substring(1) + "')"); ui.post(() -> { ttsBusy = false; restartSoon(250); }); }
                });
            }
        });
    }

    private void hideSystemUi() {
        View d = getWindow().getDecorView();
        d.setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideSystemUi();
    }

    private void js(String code) { ui.post(() -> web.evaluateJavascript(code, null)); }

    // ---------------- speech recognition ----------------
    private boolean hasMic() {
        return Build.VERSION.SDK_INT < 23 || checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED;
    }

    private void createRecognizer() {
        if (recognizer != null) { recognizer.destroy(); recognizer = null; }
        if (Build.VERSION.SDK_INT >= 31 && !onDeviceTried) {
            onDeviceTried = true;
            try { useOnDevice = SpeechRecognizer.isOnDeviceRecognitionAvailable(this); } catch (Throwable t) { useOnDevice = false; }
        }
        if (useOnDevice && Build.VERSION.SDK_INT >= 31) {
            try { recognizer = SpeechRecognizer.createOnDeviceSpeechRecognizer(this); } catch (Throwable t) { recognizer = null; useOnDevice = false; }
        }
        if (recognizer == null) {
            if (!SpeechRecognizer.isRecognitionAvailable(this)) {
                js("window.onVoiceError && window.onVoiceError('unavailable')");
                wantListen = false;
                return;
            }
            recognizer = SpeechRecognizer.createSpeechRecognizer(this);
        }
        recognizer.setRecognitionListener(new RecognitionListener() {
            @Override public void onReadyForSpeech(Bundle params) { js("window.onVoiceState && window.onVoiceState('ready')"); }
            @Override public void onBeginningOfSpeech() { }
            @Override public void onRmsChanged(float rmsdB) { }
            @Override public void onBufferReceived(byte[] buffer) { }
            @Override public void onEndOfSpeech() { }
            @Override public void onError(int error) {
                recognizing = false;
                if (error == SpeechRecognizer.ERROR_INSUFFICIENT_PERMISSIONS) {
                    wantListen = false; js("window.onVoiceError && window.onVoiceError('permission')"); return;
                }
                if (useOnDevice && (error == 12 || error == 13 || error == 10 || error == 11 || error == SpeechRecognizer.ERROR_CLIENT)) {
                    // on-device model not available for this language: fall back to the normal recogniser (offline preferred)
                    useOnDevice = false; createRecognizer(); restartSoon(200); return;
                }
                if (error == SpeechRecognizer.ERROR_NETWORK || error == SpeechRecognizer.ERROR_NETWORK_TIMEOUT || error == SpeechRecognizer.ERROR_SERVER) {
                    js("window.onVoiceError && window.onVoiceError('offline')");
                    restartSoon(2500); return;
                }
                restartSoon(error == SpeechRecognizer.ERROR_RECOGNIZER_BUSY ? 800 : 150);
            }
            @Override public void onResults(Bundle results) {
                recognizing = false;
                ArrayList<String> r = results.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION);
                if (r != null && !r.isEmpty()) {
                    JSONArray a = new JSONArray(r);
                    js("window.onVoiceFinal && window.onVoiceFinal(" + a.toString() + ")");
                }
                restartSoon(150);
            }
            @Override public void onPartialResults(Bundle partial) {
                ArrayList<String> r = partial.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION);
                if (r != null && !r.isEmpty() && r.get(0).length() > 0)
                    js("window.onVoiceResult && window.onVoiceResult(" + JSONObject.quote(r.get(0)) + ", false)");
            }
            @Override public void onEvent(int eventType, Bundle params) { }
        });
    }

    private void startRecognition() {
        if (!wantListen || ttsBusy || recognizing) return;
        if (!hasMic()) { requestPermissions(new String[]{Manifest.permission.RECORD_AUDIO}, REQ_MIC); return; }
        if (recognizer == null) createRecognizer();
        if (recognizer == null) return;
        Intent i = new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
        i.putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM);
        i.putExtra(RecognizerIntent.EXTRA_LANGUAGE, listenLang);
        i.putExtra(RecognizerIntent.EXTRA_LANGUAGE_PREFERENCE, listenLang);
        i.putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, true);
        i.putExtra(RecognizerIntent.EXTRA_MAX_RESULTS, 5);
        i.putExtra(RecognizerIntent.EXTRA_PREFER_OFFLINE, true);
        i.putExtra(RecognizerIntent.EXTRA_CALLING_PACKAGE, getPackageName());
        try { recognizer.startListening(i); recognizing = true; } catch (Throwable t) { recognizing = false; restartSoon(600); }
    }

    private final Runnable restartTask = this::startRecognition;
    private void restartSoon(long ms) { ui.removeCallbacks(restartTask); if (wantListen) ui.postDelayed(restartTask, ms); }

    private void stopRecognition() {
        ui.removeCallbacks(restartTask);
        recognizing = false;
        if (recognizer != null) { try { recognizer.cancel(); } catch (Throwable ignored) { } }
    }

    @Override
    public void onRequestPermissionsResult(int code, String[] perms, int[] res) {
        if (code == REQ_MIC) {
            if (res.length > 0 && res[0] == PackageManager.PERMISSION_GRANTED) restartSoon(100);
            else { wantListen = false; js("window.onVoiceError && window.onVoiceError('permission')"); }
        }
    }

    // ---------------- JS bridge ----------------
    class Bridge {
        @JavascriptInterface public boolean isNative() { return true; }
        @JavascriptInterface public void startListening(String lang) {
            ui.post(() -> {
                boolean langChanged = !lang.equals(listenLang);
                listenLang = lang; wantListen = true;
                if (langChanged && recognizer != null) { stopRecognition(); onDeviceTried = false; createRecognizer(); }
                recognizing = false; startRecognition();
            });
        }
        @JavascriptInterface public void stopListening() { ui.post(() -> { wantListen = false; stopRecognition(); }); }
        @JavascriptInterface public void speak(String text, String lang, boolean guard) {
            ui.post(() -> {
                if (!ttsReady) return;
                if (!guard && ttsBusy) return; // don't cut off instructions with a rep count
                String[] p = lang.split("-");
                tts.setLanguage(p.length > 1 ? new Locale(p[0], p[1]) : new Locale(p[0]));
                Bundle params = new Bundle();
                String id = (guard ? "g" : "n") + System.nanoTime();
                if (guard && wantListen) { ttsBusy = true; stopRecognition(); }
                if (Build.VERSION.SDK_INT >= 21) tts.speak(text, TextToSpeech.QUEUE_FLUSH, params, id);
            });
        }
        @JavascriptInterface public void speakEx(String text, String lang, float rate, String id) {
            ui.post(() -> {
                if (!ttsReady) { js("window.onSpeakDone && window.onSpeakDone('" + id + "')"); return; }
                String[] p = lang.split("-");
                tts.setLanguage(p.length > 1 ? new Locale(p[0], p[1]) : new Locale(p[0]));
                tts.setSpeechRate(rate);
                if (wantListen) { ttsBusy = true; stopRecognition(); }
                tts.speak(text, TextToSpeech.QUEUE_FLUSH, new Bundle(), "g" + id);
                tts.setSpeechRate(1.0f);
            });
        }
        @JavascriptInterface public void stopSpeaking() { ui.post(() -> { if (ttsReady) tts.stop(); ttsBusy = false; restartSoon(200); }); }
    }

    @Override
    protected void onPause() {
        super.onPause();
        stopRecognition();
        if (ttsReady) tts.stop();
        web.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        web.onResume();
        hideSystemUi();
        if (wantListen) restartSoon(400);
    }

    @Override
    protected void onDestroy() {
        if (recognizer != null) recognizer.destroy();
        if (tts != null) tts.shutdown();
        web.destroy();
        super.onDestroy();
    }
}
