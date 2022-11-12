package com.example.h264_client;

import android.content.res.AssetManager;
import android.util.Log;
import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import android.content.Context;
import android.os.Bundle;

public class H264ClientJni {
    static {
        System.loadLibrary("h264_client");
    }

    public static final int TRANSPORT_PROTOCOL_RTSP_MULTICAST = 0;
    public static final int TRANSPORT_PROTOCOL_NOVO_TCP = 1;

    public static final int MEDIA_FRAME_FMT_H264    = 0;
    public static final int MEDIA_FRAME_FMT_H265    = 1;
    public static final int MEDIA_FRAME_FMT_PCM     = 10;
    public static final int MEDIA_FRAME_FMT_AAC     = 11;
    public static final int MEDIA_FRAME_FMT_AC3     = 12;

    public static class FrameInfo {
        public int fmt_;                // MEDIA_FRAME_FMT_H264, ...

        // video
        public int frame_width_;
        public int frame_height_;
        public int is_i_frame_;
        public int debug_frame_no_;
    }

    private static OnH264ClientJniListener ms_listener;

    public static void setOnListener(OnH264ClientJniListener listener) {
        ms_listener = listener;
    }

    // native interface
    /**
     * create a stream client
     *
     * @param ip:        server ip. ex: 192.168.5.1
     * @param trans_proto: transport protocol.
     *                   ex: 0 => TRANSPORT_PROTOCOL_RTSP_MULTICAST
     *                   1 => TRANSPORT_PROTOCOL_NOVO_TCP
     * @return:          stream id, 0 => error.
     *
     */
    public static native int CreateClient(String ip, int trans_proto, String username, String password);

    /**
     * start a stream client
     *
     * @param sid:        stream id.
     *
     */
    public static native void StartClient(int sid);

    public static native void DestroyClient(int sid);

    public static native void DestroyAllClient();

    // callback
    public static void OnFrame(int sid, ByteBuffer frame, int frame_len, FrameInfo frame_info) {
        ms_listener.OnFrame(sid, frame, frame_len, frame_info);
    }

    public static void OnPacket(int sid, int kbitrate) {
        ms_listener.OnPacket(sid, kbitrate);
    }

    /**
     * stream client status
     *
     * @param sid:        stream id.
     * @param status:     status id:    0=> STARTED (stream has started.)
     *                                  1=> INTERRUPTING (stream is interrupting, notify callback each 5 sec..)
     *                                  2=> DESTROYED (stream has been destroyed.)
     *
     */
    public static void OnStatus(int sid, int status) {
        ms_listener.OnStatus(sid, status);
    }
}
