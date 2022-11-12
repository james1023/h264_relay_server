
package com.example.h264_client;

import java.nio.ByteBuffer;
import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;
import android.content.Context;
import android.content.ContentResolver;
import android.content.res.AssetManager;
import android.net.Uri;
import android.database.Cursor;
import android.provider.MediaStore.Images.ImageColumns;

import com.example.h264_client.H264ClientJni.FrameInfo;

public class H264ClientTest extends Activity {

    private static final String TAG = "H264ClientTest";

    private static H264ClientJni client_jni_;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        client_jni_.setOnListener(new OnH264ClientJniListener() {
            @Override
            public void OnFrame(int sid, ByteBuffer frame, int frame_len, FrameInfo frame_info)
            {
                if (frame != null) {
                    byte[] data = new byte[frame_len];
                    frame.get(data);

                    Log.d(TAG, "OnFrame: sid=" + sid + ", img=" + data[0] + data[1] + data[2] + data[3] + data[4] +
                            ",len=" + frame_len + ",fmt=" + frame_info.fmt_ + ",w=" + frame_info.frame_width_ + ",h=" + frame_info.frame_height_ + ",debug_frame_no_=" + frame_info.debug_frame_no_);
                }
            }

            @Override
            public void OnPacket(int sid, int kbitrate)
            {
                Log.d(TAG, "OnPacket: sid=" + sid + ", kbitrate=" + kbitrate);
            }

            @Override
            public void OnStatus(int sid, int status)
            {
                Log.d(TAG, "OnStatus: sid=" + sid + ", status=" + status);
            }
        });

        int sid = client_jni_.CreateClient("192.168.5.7", H264ClientJni.TRANSPORT_PROTOCOL_NOVO_TCP, "", "");
        client_jni_.StartClient(sid);
    }


}
