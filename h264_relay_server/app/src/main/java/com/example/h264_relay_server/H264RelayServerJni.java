
package com.example.h264_relay_server;

import java.nio.ByteBuffer;
import android.content.res.AssetManager;

public class H264RelayServerJni {
    static {
        System.loadLibrary("h264_relay_server");
    }

    public static interface OnEventListener {
        public void OnStatus(String addr, int status);
    }

    private static OnEventListener event_listener_ = null;

    //////////////////////////////////////////////////////////////////////////////////////
    // interface

    public static void setOnEventListener(OnEventListener listener) {
        event_listener_ = listener;
    }


    /**
     * create a server
     *
     * @return boolean:     create the server that is failed.
     */
    public static native boolean CreateServer();

    /**
     * Release a server
     *
     */
    public static native void ReleaseServer();


    public static final int MEDIA_FRAME_FMT_H264    = 0;

    /**
     * push a video frame to server
     *
     * @param fmt:          media frame format:     MEDIA_FRAME_FMT_H264, ...
     * @param frame:        frame buffer (from java.nio.ByteBuffer.allocateDirect
     * @param frame_len:    frame length
     */

    public static native void PushVideoFrame(int fmt, ByteBuffer frame, int frame_len);

    // callback
    /**
     * stream client status
     *
     * @param addr:        client addr => ip:port.
     * @param status:      status id:    STATUS_CLIENT_STARTED => client has started.
     *                                   STATUS_CLIENT_DESTROYED => client has been destroyed.
     *
     */
    public static final int STATUS_CLIENT_STARTED = 0;
    public static final int STATUS_CLIENT_DESTROYED = 1;

    public static void OnStatus(String addr, int status) {
        if (event_listener_ != null)
            event_listener_.OnStatus(addr, status);
    }

    public static native void EnableUnittest(AssetManager assetManager, String file_path);
}
