package com.gstreamertest;

import android.util.Log;
import com.facebook.react.ReactPackage;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.NativeModule;
import com.facebook.react.uimanager.ViewManager;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class RCTGstPlayerPackage implements ReactPackage {

    private static final String TAG = "RCTGstPlayerPackage";

    @Override
    public List<NativeModule> createNativeModules(ReactApplicationContext reactContext) {
        Log.d(TAG, "createNativeModules called");
        return Collections.emptyList();
    }

    @Override
    public List<ViewManager> createViewManagers(ReactApplicationContext reactContext) {
        Log.d(TAG, "createViewManagers called");
        List<ViewManager> viewManagers = new ArrayList<>();
        viewManagers.add(new RCTGstPlayer());
        return viewManagers;
    }
}
