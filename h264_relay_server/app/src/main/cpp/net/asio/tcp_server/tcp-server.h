#pragma once

#include <string>
#include <map>
#include <mutex>

#include "live_stream_server-c.h"
#include "server.hpp"

namespace net {

class TcpServer
{
public:
	static TcpServer &Instance() {
		static TcpServer instance;
		return instance;
	}

	void ServerThread(std::string port, std::function<OnStreamServerMsg> on_msg, const void *usr_arg);

	bool CreateServer(std::string port, std::function<OnStreamServerMsg> on_msg, const void *usr_arg);
	void Release();
	void ReleaseServer();
	void PushFrame(MEDIA_FRAME_FMT fmt, const unsigned char *frame_buff, int frame_len);
	void EnableUnittest(const unsigned char *buff, int buff_len);

	/*std::function<OnServerMsg> &msg_func() {
		return on_http_server_msg_;
	}
	void set_msg_func(std::function<OnServerMsg> &in) {
		on_http_server_msg_ = in;
	}*/

private:
	explicit TcpServer();
	virtual ~TcpServer();

	TcpServer(const TcpServer &) = delete;
	TcpServer &operator=(const TcpServer &) = delete;

private:
    std::shared_ptr<std::thread> server_thread_;
	std::shared_ptr<net::tcp::server::server> server_;
    std::mutex interface_lock_;

	//std::function<OnServerMsg> on_http_server_msg_;

    bool enable_unittest_;
};

} // namespace net
