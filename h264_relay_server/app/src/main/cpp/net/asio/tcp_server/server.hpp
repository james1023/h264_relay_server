#pragma once

#include <string>
#include <vector>
#include <functional>

#include "net/check_stream.h"

#include "connection_manager.hpp"
#include "connection.hpp"
#include "request_handler.hpp"

namespace net {
namespace tcp {
namespace server {

/// The top-level class of the HTTP server.
class server : public std::enable_shared_from_this<server>
{
public:
	/// Construct the server to listen on the specified TCP address and port, and
	/// serve up files from the given directory.
	explicit server(const std::string& address, const std::string& port,
		const std::string& doc_root, std::size_t thread_pool_size,
		std::function<OnStreamServerMsg> on_msg, const void *usr_arg);

	virtual ~server();

	/// Run the server's io_service loop.
	void run();
    
    void stop();

	void handle_stop_clients();

	void PushFrame(MEDIA_FRAME_FMT fmt, const unsigned char *frame_buff, int frame_len);

	void OnStreamEvent();

private:
	/// Initiate an asynchronous accept operation.
	void start_accept();

	/// Handle completion of an asynchronous accept operation.
	void handle_accept(const std::error_code& e);

	/// Handle a request to stop the server.
	void handle_close_server();

	/// The number of threads that will call io_service::run().
	std::size_t thread_pool_size_;

	/// The io_service used to perform asynchronous operations.
	asio::io_context io_context_;

	/// The signal_set is used to register for process termination notifications.
	asio::signal_set signals_;

	/// Acceptor used to listen for incoming connections.
	asio::ip::tcp::acceptor acceptor_;

	/// The connection manager which owns all live connections.
	connection_manager connection_manager_;

	/// The next connection to be accepted.
	connection_ptr new_connection_;

	/// The handler for all incoming requests.
	request_handler request_handler_;

    std::shared_ptr<asio::steady_timer> event_handle_;
	unsigned int kp_count_pfrm_;
	net::CheckStreamRate kp_frm_timeout_checker_;

    std::mutex last_I_frm_lock_;
    unsigned char *last_I_frm_;
    unsigned int last_I_frm_len_;
    unsigned int last_I_frm_mem_len_;

	std::function<OnStreamServerMsg> on_msg_;
	const void *usr_arg_;
};

} // namespace server
} // namespace tcp
} // namespace net
