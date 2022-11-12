#pragma once

#include <mutex>

#include <asio.hpp>
#include "circular-1.0/bounded_buffer.h"
#include "net/check_stream.h"

#include "live_stream_server-c.h"
#include "reply.hpp"
#include "request.hpp"
#include "request_handler.hpp"
#include "request_parser.hpp"

namespace net {
namespace tcp {
namespace server {

enum FFS_MEDIA_TYPE {
    FFS_MEDIA_TYPE_VIDEO		= 0,
    FFS_MEDIA_TYPE_AUDIO,
};

enum FFSEncodeFrameType {
	FFS_ENCODE_FRAME_TYPE_I		= 0,
	FFS_ENCODE_FRAME_TYPE_P,
};

struct FrameFactoryStorage {
	unsigned char *frame_buff;
	int frame_len;
	int frame_mem_len;

    FFS_MEDIA_TYPE mt;
    MEDIA_FRAME_FMT fmt;

    // video
    FFSEncodeFrameType encode_frame_type;
};

class connection_manager;

/// Represents a single connection from a client.
class connection
  : public std::enable_shared_from_this<connection>
{
	#define QUEUE_SIZE 40

	#define RESERVE_HEAD_SIZE 256

public:
	/// Construct a connection with the given io_service.
	explicit connection(asio::io_context &io_context,
					  connection_manager& manager, request_handler& handler,
					  std::function<OnStreamServerMsg> on_msg, const void *usr_arg);

	virtual ~connection();

	void handle_data(const std::error_code &e, std::size_t bytes_transferred);

	/// Get the socket associated with the connection.
	asio::ip::tcp::socket& socket();

	/// Start the first asynchronous operation for the connection.
	void start();

	/// Stop all asynchronous operations associated with the connection.
	void stop();

	virtual void PushFrame(const FrameFactoryStorage *frame);
	virtual void OnPushNotify(const FrameFactoryStorage *new_val, FrameFactoryStorage *org_val);
	virtual void PopFrame();
	virtual void OnPopNotify(const FrameFactoryStorage *org_val, FrameFactoryStorage *out_val, int un_read);

    virtual bool has_first_idr() {
        return has_first_idr_;
    }
    void set_has_first_idr(bool in) {
        has_first_idr_ = in;
    }

	virtual bool notify_lose_frame() {
		return notify_lose_frame_;
	}
	void set_notify_lose_frame(bool in) {
		notify_lose_frame_ = in;
	}

private:

	asio::io_context &io_context_;
 
	/// Socket for the connection.
	asio::ip::tcp::socket socket_;

    std::string local_ip_;
    int local_port_;

	/// The manager for this connection.
	connection_manager &connection_manager_;

	/// The handler used to process the incoming request.
	request_handler &request_handler_;

    unsigned char *async_stream_;
    unsigned int async_stream_mem_len_;

	/// The incoming request.
	request request_;

	/// The parser for the incoming request.
	request_parser request_parser_;

	/// The reply to be sent back to the client.
	reply reply_;

	std::function<OnStreamServerMsg> on_msg_;
	const void *usr_arg_;

	bounded_buffer<FrameFactoryStorage> bounded_frame_;
    bool has_first_idr_;
    std::mutex old_frame_no_mtx;
	unsigned char old_frame_no_;
	net::CheckStreamRate video_fame_no_limit_checker_;
	unsigned char video_frame_no_;
	bool notify_lose_frame_;
};

typedef std::shared_ptr<connection> connection_ptr;

} // namespace server
} // namespace tcp
} // namespace novo
