package com.gstreamertest;

import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.View;
import android.widget.Toast;

import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.ReactContext;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.uimanager.events.RCTEventEmitter;
import com.gstreamertest.utils.EaglUIView;
import com.gstreamertest.utils.RCTGstConfigurationCallable;
import com.gstreamertest.utils.RCTGstConfiguration;

import org.freedesktop.gstreamer.GStreamer;

/**
 * Created by vishal singh on 16/o6/2024.
 */

public class RCTGstPlayerController implements RCTGstConfigurationCallable, SurfaceHolder.Callback {

    private static final String LOG_TAG = "RCTGstPlayerController";

    private boolean isInited = false;

    private RCTGstConfiguration configuration;
    private EaglUIView view;
    private ReactContext context;

    // Native methods
    private native String nativeRCTGstGetGStreamerInfo();
    private native void nativeRCTGstSetDrawableSurface(Surface drawableSurface);
    private native void nativeRCTGstSetUri(String uri);
    private native void nativeRCTGstSetAudioLevelRefreshRate(int audioLevelRefreshRate);
    private native void nativeRCTGstSetDebugging(boolean isDebugging);
    private native void nativeRCTGstSetPipelineState(int state);
    private native void nativeRCTGstInitAndRun(RCTGstConfiguration configuration);

    // Configuration callbacks
    @Override
    public void onInit() {
        Log.d(LOG_TAG, "onInit() called");
        context.getJSModule(RCTEventEmitter.class).receiveEvent(
                view.getId(), "onPlayerInit", null
        );
    }

    @Override
    public void onStateChanged(int old_state, int new_state) {
        Log.d(LOG_TAG, "onStateChanged() called with old_state: " + old_state + ", new_state: " + new_state);
        WritableMap event = Arguments.createMap();
        event.putInt("old_state", old_state);
        event.putInt("new_state", new_state);
        context.getJSModule(RCTEventEmitter.class).receiveEvent(
                view.getId(), "onStateChanged", event
        );
    }

    @Override
    public void onVolumeChanged(double rms, double peak, double decay) {
        Log.d(LOG_TAG, "onVolumeChanged() called with rms: " + rms + ", peak: " + peak + ", decay: " + decay);
        WritableMap event = Arguments.createMap();
        event.putDouble("rms", rms);
        event.putDouble("peak", peak);
        event.putDouble("decay", decay);
        context.getJSModule(RCTEventEmitter.class).receiveEvent(
                view.getId(), "onVolumeChanged", event
        );
    }

    @Override
    public void onUriChanged(String new_uri) {
        Log.d(LOG_TAG, "onUriChanged() called with new_uri: " + new_uri);
        WritableMap event = Arguments.createMap();
        event.putString("new_uri", new_uri);
        context.getJSModule(RCTEventEmitter.class).receiveEvent(
                view.getId(), "onUriChanged", event
        );
    }

    @Override
    public void onEOS() {
        Log.d(LOG_TAG, "onEOS() called");
        context.getJSModule(RCTEventEmitter.class).receiveEvent(
                view.getId(), "onEOS", null
        );
    }

    @Override
    public void onElementError(String source, String message, String debug_info) {
        Log.d(LOG_TAG, "onElementError() called with source: " + source + ", message: " + message + ", debug_info: " + debug_info);
        WritableMap event = Arguments.createMap();
        event.putString("source", source);
        event.putString("message", message);
        event.putString("debug_info", debug_info);
        context.getJSModule(RCTEventEmitter.class).receiveEvent(
                view.getId(), "onElementError", event
        );
    }

    // Surface callbacks
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.d(LOG_TAG, "surfaceCreated() called");
        if (!isInited) {
            Log.d(LOG_TAG, "Initializing GStreamer with surface: " + holder.getSurface());
            // Preparing configuration
            this.configuration.setInitialDrawableSurface(holder.getSurface());
            // Init and run our pipeline
            nativeRCTGstInitAndRun(this.configuration);
            // Init done
            this.isInited = true;
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.d(LOG_TAG, "surfaceChanged() called with format: " + format + ", width: " + width + ", height: " + height);
        nativeRCTGstSetDrawableSurface(holder.getSurface());
        Log.d(LOG_TAG, "surfaceChanged() called with formattttttttttttttttttttttttttttttt: " + format + ", width: " + width + ", height: " + height);

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.d(LOG_TAG, "surfaceDestroyed() called");
    }

    // Constructor
   public RCTGstPlayerController(ReactContext context) {
    Log.d(LOG_TAG, "RCTGstPlayerController() constructor called");
    this.context = context;
    try {
        GStreamer.init(context);
        Log.d(LOG_TAG, "GStreamer initialized successfully");
    } catch (Exception e) {
        Log.e(LOG_TAG, "Error initializing GStreamer: " + e.getMessage());
        Toast.makeText(context, e.getMessage(), Toast.LENGTH_LONG).show();
    }
    String version = nativeRCTGstGetGStreamerInfo();
    Log.d(LOG_TAG, "GStreamer version: " + version);
    this.view = new EaglUIView(this.context, this);
    this.configuration = new RCTGstConfiguration(this);
}

// @Override
// public void onUriChanged(String new_uri) {
//     if (new_uri != null) {
//         Log.d(LOG_TAG, "onUriChanged() called with new_uri: " + new_uri);
//     } else {
//         Log.d(LOG_TAG, "onUriChanged() called with null new_uri");
//     }
//     WritableMap event = Arguments.createMap();
//     event.putString("new_uri", new_uri);
//     context.getJSModule(RCTEventEmitter.class).receiveEvent(
//             view.getId(), "onUriChanged", event
//     );
// }


    View getView() {
        Log.d(LOG_TAG, "getView() called");
        return this.view;
    }

    // Manager Shared properties
    void setRctGstUri(String uri) {
        Log.d(LOG_TAG, "setRctGstUri() called with uri: " + uri);
        nativeRCTGstSetUri(uri);
    }


    void setRctGstAudioLevelRefreshRate(int audioLevelRefreshRate) {
        Log.d(LOG_TAG, "setRctGstAudioLevelRefreshRate() called with rate: " + audioLevelRefreshRate);
        nativeRCTGstSetAudioLevelRefreshRate(audioLevelRefreshRate);
    }

    void setRctGstDebugging(boolean isDebugging) {
        Log.d(LOG_TAG, "setRctGstDebugging() called with isDebugging: " + isDebugging);
        nativeRCTGstSetDebugging(isDebugging);
    }

    // Manager methods
    void setRctGstState(int state) {
        Log.d(LOG_TAG, "setRctGstState() called with state: " + state);
        nativeRCTGstSetPipelineState(state);
    }

    // External C Libraries
    static {
        Log.d(LOG_TAG, "Loading external C libraries");
        System.loadLibrary("gstreamer_android");
        System.loadLibrary("rctgstplayer");
    }
}
