package com.gstreamertest.utils;

import android.content.Context;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/**
 * Created by vishal singh on 16/o6/2024.
 */

public class EaglUIView extends SurfaceView {

    public Surface getHandle() {
        return this.getHolder().getSurface();
    }

    public EaglUIView(Context context, SurfaceHolder.Callback sufaceHolderManager) {
        super(context);
        this.getHolder().addCallback(sufaceHolderManager);
    }
}
