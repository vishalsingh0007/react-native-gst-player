package com.gstreamertest.utils;

/**
 * Created by vishal singh on 16/o6/2024.
 */

public interface RCTGstConfigurationCallable {
    // Called when the player is ready
    void onInit();

    // Called method when GStreamer state changes
    void onStateChanged(int old_state, int new_state);

    // Called method when current media volume changes
    void onVolumeChanged(double rms, double peak, double decay);

    // Called when changing uri is over
    void onUriChanged(String new_uri);

    // Called when EOS occurs
    void onEOS();

    // Called when an error occurs
    void onElementError(String source, String message, String debug_info);

}
