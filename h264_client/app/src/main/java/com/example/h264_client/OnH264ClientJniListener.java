
package com.example.h264_client;

import java.nio.ByteBuffer;
import com.example.h264_client.H264ClientJni.FrameInfo;

public interface OnH264ClientJniListener {

    public void OnFrame(int sid, ByteBuffer frame, int frame_len, FrameInfo frame_info);

    public void OnPacket(int sid, int kbitrate);

    public void OnStatus(int sid, int status);
}