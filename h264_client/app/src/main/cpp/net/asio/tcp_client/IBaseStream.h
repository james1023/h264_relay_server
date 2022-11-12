#ifndef IBASE_STREAM_H
#define IBASE_STREAM_H

#include <memory>

#include "net/asio/tcp_client/stream-client-i.h"
#include "net/asio/tcp_client/stream_live.pb.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef enum _BASE_ERR
{
	BASE_OK = 1,
	BASE_CONNECT_FAILED = 0,
	BASE_HTTP_401_Unauthorized = -1,
	BASE_PARAM_ERROR = -2,
	BASE_USERLIMIT_ERROR = -3,
	BASE_IMGRES_NOSUPT = -4,
	BASE_FRAME_OVERFLOW_ERROR = -5,
	BASE_ENCODING_NOSUPT = -6,
	BASE_SERVER_OVER = -7,
	BASE_MEMORY_ERROR = -8,
	BASE_FRAME_ERROR = -9,
	BASE_IO_ERROR = -10,
	BASE_RECV_ERROR = -11,
	BASE_RECV_AGAIN_ERROR = -12,
	BASE_INIT_ERROR = -13,
	BASE_HTTP_404_Not_Found = -14,
	BASE_STREAM_WARNING_TIMEOUT = -20,
} BASE_ERR;

typedef enum _BASESTREAM_PROTOCOL_MODE
{
	BASESTREAM_PROTOCOL_HTTP = 0x00,
	BASESTREAM_PROTOCOL_TCP = 0x01,
	BASESTREAM_PROTOCOL_UDP = 0x02,
} BASESTREAM_PROTOCOL_MODE;

typedef enum _BASESTREAM_FORMAT
{
	BASESTREAM_FORMAT_H264 = 0x00,
	BASESTREAM_FORMAT_MPEG4 = 0x01,
	BASESTREAM_FORMAT_JPEG = 0x02,
	BASESTREAM_FORMAT_ULAW = 0x03,
	BASESTREAM_FORMAT_H265 = 0x04,
} BASESTREAM_FORMAT;

class IBaseStream :	public std::enable_shared_from_this<IBaseStream>
{
public:
	virtual void set_callback(OnFrameCb *on_frame, OnStatusCb *on_status, const void *usr_arg) = 0;
	virtual OnFrameCb *on_frame() = 0;
	virtual OnStatusCb *on_status() = 0;
	
	virtual void set_packet_cb(OnPacketCb *on_packet) = 0;
	virtual OnPacketCb *on_packet() = 0;

	virtual net::StreamMessage &stream_login_message() = 0;
	virtual void set_stream_login_message(net::StreamMessage &in) = 0;

	virtual void Connect() = 0;
	virtual void AsyncConnect() = 0;
	virtual BASE_ERR Disconnect() = 0;
	
	virtual void set_stream_id(unsigned int strm_id) = 0;
	virtual unsigned int stream_id() = 0;

	virtual bool notify_disconnect() = 0;
	virtual void set_notify_disconnect(bool in) = 0;
	virtual bool notify_remove() = 0;
	virtual void set_notify_remove(bool in) = 0;
};

#ifdef __cplusplus
}
#endif // __cplusplus

#endif	// IBASE_STREAM_H
