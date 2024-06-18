package com.gstreamertest;

import androidx.annotation.Nullable;
import android.util.Log;
import android.view.View;

import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.common.MapBuilder;
import com.facebook.react.uimanager.SimpleViewManager;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.annotations.ReactProp;
import com.gstreamertest.utils.manager.Command;
import com.gstreamertest.RCTGstPlayerController;

import java.util.Map;

/**
 * Created by vishal singh on 16/o6/2024.
 */

public class RCTGstPlayer extends SimpleViewManager<View> {

    private static final String LOG_TAG = "RCTGstPlayer";
    private RCTGstPlayerController playerController;

    @Override
    public String getName() {
        Log.d(LOG_TAG, "getName() called");
        return "RCTGstPlayer";
    }

    @Override
    protected View createViewInstance(ThemedReactContext reactContext) {
        Log.d(LOG_TAG, "createViewInstance() called");
        this.playerController = new RCTGstPlayerController(reactContext);
        return this.playerController.getView();
    }

    // Shared properties
    @ReactProp(name = "uri")
    public void setUri(View controllerView, String uri) {
        Log.d(LOG_TAG, "setUri() called with uri: " + uri);
        this.playerController.setRctGstUri(uri);
    }

    @ReactProp(name = "isDebugging")
    public void setIsDebugging(View controllerView, boolean isDebugging) {
        Log.d(LOG_TAG, "setIsDebugging() called with isDebugging: " + isDebugging);
        this.playerController.setRctGstDebugging(isDebugging);
    }

    // Methods
    @Override
    public void receiveCommand(View view, int commandType, @Nullable ReadableArray args) {
        Log.d(LOG_TAG, "receiveCommand() called with commandType: " + commandType);
        if (args != null) {
            Log.d(LOG_TAG, "receiveCommand() called with args: " + args.toString());
        }

        // setState
        if (Command.is(commandType, Command.setState)) {
            this.playerController.setRctGstState(args.getInt(0));
        }

        // recreateView is ignored on purpose : Not needed on android (wrong impl of vtdec on ios)
    }

    // Commands (JS callable methods listing map)
    @Override
    public Map<String, Integer> getCommandsMap() {
        Log.d(LOG_TAG, "getCommandsMap() called");
        return Command.getCommandsMap();
    }

    /**
     * This method maps the sending of the "onPlayerInit" event to the JS "onPlayerInit" function.
     */

    // Events callbacks
    @Nullable
    @Override
    public Map<String, Object> getExportedCustomDirectEventTypeConstants() {
        Log.d(LOG_TAG, "getExportedCustomDirectEventTypeConstants() called");
        return MapBuilder.<String, Object>builder()
                .put(
                        "onPlayerInit", MapBuilder.of("registrationName", "onPlayerInit")
                ).put(
                        "onStateChanged", MapBuilder.of("registrationName", "onStateChanged")
                ).put(
                        "onUriChanged", MapBuilder.of("registrationName", "onUriChanged")
                ).put(
                        "onEOS", MapBuilder.of("registrationName", "onEOS")
                ).put(
                        "onElementError", MapBuilder.of("registrationName", "onElementError")
                ).build();
    }
}
