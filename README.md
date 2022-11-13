# h264_relay_server
h264 stream relay server by jni on Android

#
# server
#
private static H264RelayServerJni server_jni_;

server_jni_.CreateServer();

server_jni_.setOnEventListener(new H264RelayServerJni.OnEventListener() {
            @Override
            public void OnStatus(String addr, int status) {
                Log.d(TAG, "OnStatus: addr=" + addr + ", status=" + status);
            }
        });

server_jni_.PushVideoFrame(H264RelayServerJni.MEDIA_FRAME_FMT_H264, data, len);


#
# client
#
private static H264ClientJni client_jni_;

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
}
            
int sid = client_jni_.CreateClient("192.168.1.1", H264ClientJni.TRANSPORT_PROTOCOL_NOVO_TCP, "", "");

client_jni_.StartClient(sid);
