#pragma once

#include <asio.hpp>

#include "net/check_stream.h"
#include "media/h264/h264_stream.h"
#include "net/asio/event/event_proc.h"
#include "net/asio/tcp_client/stream-client-i.h"
#include "net/asio/tcp_client/IBaseStream.h"
#include "net/asio/tcp_client/stream_live.pb.h"

class StreamManager;

class CTCPStream : public IBaseStream			
{
#define STREM_HEAD_FIRST_SIZE			8
private:
	unsigned int strm_id_;
	
	OnFrameCb *on_frame_;
	OnStatusCb *on_status_;
	OnPacketCb *on_packet_;
	const void *usr_arg_;

	bool old_screen_format_;
	
	unsigned int now_frame_size_;
	unsigned int now_head_size_;
	MEDIA_FRAME_FMT now_fmt_;

	unsigned int timeout_frame_num_;
	net::CheckStreamRate timeout_stream_check_;

	net::CheckStreamRate losefrm_limit_checker_;

	net::AsioEventService &aev_service_;
	asio::ip::tcp::socket stream_tcp_socket_;
	std::shared_ptr<asio::steady_timer> event_handle_;
	std::recursive_mutex  event_guard_;

	std::shared_ptr<unsigned char> stream_buff_;
	int stream_buff_size_;

	//third_party::avtech::rtp_extension::FrameMessage frame_msg_;
	bool has_first_frame_;

	int stream_pre_frame_no_;
	unsigned int stream_frame_ok_count_;

	int notify_disconnect_count_;
    bool notify_start_live_;
    bool notify_disconnect_;
    bool notify_remove_;

	StreamManager &stream_manager_;
	net::StreamMessage strm_login_msg_;

public:
	CTCPStream(StreamManager &stream_manager);
	virtual ~CTCPStream();

	virtual void Connect();
	virtual void AsyncConnect();

	virtual void set_callback(OnFrameCb *on_frame, OnStatusCb *on_status, const void *usr_arg) {
		on_frame_ = on_frame;
		on_status_ = on_status;
		usr_arg_ = usr_arg;
	}

	virtual void set_packet_cb(OnPacketCb *on_packet) {
		on_packet_ = on_packet;
	}

	virtual OnFrameCb *on_frame() {
		return on_frame_;
	}

	virtual OnStatusCb *on_status() {
		return on_status_;
	}

	virtual OnPacketCb *on_packet() {
		return on_packet_;
	}

	// Interface.
	void set_stream_id(unsigned int strm_id) {
		strm_id_ = strm_id;
	}
	unsigned int stream_id() {
		return strm_id_;
	}

	BASE_ERR Connect(const char *pchUrlName, const char *pchUserName, const char *pchPassword);
	BASE_ERR Disconnect();

	void HandleConnect(const std::error_code &e);
	void HandleStreamFirst(const std::error_code &e, std::size_t bytes_transferred);
    void HandleStreamHead(const std::error_code &e, std::size_t bytes_transferred);
	void HandleStreamBody(const std::error_code &e, std::size_t bytes_transferred);

	void CheckStreamEvent();
	void StreamEventHandle();
	//void OnAsyncCallback(std::function<void(void)> on_callback_bind);

	bool has_first_frame() {
		return has_first_frame_;
	}
	void set_has_first_frame(bool in) {
		has_first_frame_ = in;
	}
    
    virtual bool notify_disconnect() {
        return notify_disconnect_;
    }
    void set_notify_disconnect(bool in) {
        notify_disconnect_ = in;
    }

	virtual bool notify_remove() {
		return notify_remove_;
	}
	virtual void set_notify_remove(bool in) {
		notify_remove_ = in;
	}
	// ~Interface.

	virtual net::StreamMessage &stream_login_message() {
		return strm_login_msg_;
	}
	virtual void set_stream_login_message(net::StreamMessage &in) {
		strm_login_msg_ = in;
	}
};
