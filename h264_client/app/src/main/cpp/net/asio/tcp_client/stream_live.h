
#if !defined __STREAM_LIVE_H__
#define __STREAM_LIVE_H__

#include <condition_variable>
#include "base/debug-msg-c.h"
#include "net/asio/tcp_client/stream-client-i.h"
#include "net/asio/tcp_client/stream_manager.h"

class StreamLive
{
private:
    explicit StreamLive();
    virtual ~StreamLive();
    
    StreamLive(const StreamLive&) = delete;
    StreamLive & operator=(const StreamLive&) = delete;
    
    StreamManager stream_manager_;
    
    // TCPStreamThread
    std::shared_ptr<std::thread> tcp_stream_live_thread_;
    std::mutex tcp_stream_live_mutex_;
    std::condition_variable_any tcp_stream_live_cond_;
    bool tcp_stream_live_cond_exit_;
    
    unsigned long tcp_loop_count_;
    int tcp_msg_status_;
    
public:
    static StreamLive& Instance() {
        static StreamLive instance;
        return instance;
    }
    
	// interface
	unsigned int CreateClient(const char *ip, TRANSPORT_PROTOCOL_TYPE trans_proto, const char *username, const char *password,
							  OnFrameCb *on_frame, OnStatusCb *on_status, const void *usr_arg);

	bool StartClient(unsigned int sid);

	bool DestroyClient(unsigned int sid);

    bool DestroyAllClient();
    
	bool SetPacketCB(unsigned int sid, OnPacketCb on_packet);

    // TCPStreamThread
    std::shared_ptr<std::thread> &tcp_stream_live_thread() {
        return tcp_stream_live_thread_;
    }
    void TCPStreamLiveThread();
    int BeginTCPStreamLiveThread();
    int EndTCPStreamLiveThread();
    
    void setTCPMsgSts(int msg) {
        tcp_msg_status_ = msg;
    }
    int getTCPMsgSts() {
        return tcp_msg_status_;
    }
};

#endif // __STREAM_LIVE_H__
