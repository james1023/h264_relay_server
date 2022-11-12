#include <string>
#include <vector>

#include "base/debug-msg-c.h"
#include "net/check_stream.h"
#include "server.hpp"

#define EVENT_INTVAL                            1
#define KEEP_PUSH_FRAME_INTVAL                  3

namespace net {
namespace tcp {
namespace server {

bool check_nal_type(unsigned char *buf, int size, int *nal_type, int *offset, int *start_code)
{
    int i = 0, sc = 0;

    while (   //( next_bits( 24 ) != 0x000001 && next_bits( 32 ) != 0x00000001 )
            (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) &&
            (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0 || buf[i+3] != 0x01)
            )
    {
        i++; // skip leading zero
        if (i+4 >= size) {
            return false;
        } // did not find nal start
    }

    if (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) // ( next_bits( 24 ) != 0x000001 )
    {
        i++;
        sc++;
    }

    if (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) {
        /* error, should never happen */
        return false;
    }
    i += 3;
    sc += 3;

    if (nal_type) *nal_type = *(buf+i) & 0x1F;

    if (offset) *offset = i;
    if (start_code) *start_code = sc;

    return true;
}

server::server(const std::string& address, const std::string& port,
    const std::string& doc_root, std::size_t thread_pool_size, 
	std::function<OnStreamServerMsg> on_msg, const void *usr_arg)
  : io_context_(),
	signals_(io_context_),
	acceptor_(io_context_),
	thread_pool_size_(thread_pool_size),
    connection_manager_(),
    new_connection_(),
    request_handler_(doc_root),
    kp_count_pfrm_(0),
    last_I_frm_(NULL),
    last_I_frm_len_(0),
    last_I_frm_mem_len_(0),
	on_msg_(on_msg),
	usr_arg_(usr_arg)
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) server(%#x) in. \n", __LINE__, this);
    
    // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
    // 20170811 james.ch: use local ip.
#if 0
    asio::ip::tcp::resolver resolver(io_service_);
    asio::ip::tcp::resolver::query query(address, port);
    asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);
#else
    int port_val = std::stoi(port);
    asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port_val);
#endif
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();
    
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) server(%#x) out. \n", __LINE__, this);
}

server::~server() {
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) ~server(%#x) in. \n", __LINE__, this);

    stop();
    
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) ~server(%#x) out. \n", __LINE__, this);
}

void server::run()
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) server::run: in. \n", __LINE__);
    
    // Register to handle the signals that indicate when the server should exit.
    // It is safe to register for the same signal multiple times in a program,
    // provided all registration for the specified signal is made through Asio.
    signals_.add(SIGINT);
    signals_.add(SIGTERM);
#if defined(SIGQUIT)
    signals_.add(SIGQUIT);
#endif // defined(SIGQUIT)

    // 20170825 james.ch: it cannot use shared_from_this() in constructor.
    signals_.async_wait(std::bind(&server::handle_close_server, shared_from_this()));
    
    start_accept();
    
    // Create a pool of threads to run all of the io_services.
    std::vector<std::shared_ptr<std::thread> > threads;

	std::size_t(asio::io_context::*io_context_run)() = &asio::io_context::run;
    for (std::size_t i = 0; i < thread_pool_size_; ++i)
    {
        std::shared_ptr<std::thread> thread(new std::thread(
            std::bind(io_context_run, &io_context_)));
        threads.push_back(thread);
    }

    event_handle_.reset(new asio::steady_timer(io_context_));
    event_handle_->expires_after(std::chrono::seconds(EVENT_INTVAL));

    auto self(shared_from_this());
    event_handle_->async_wait(
            [this, self](const std::error_code &ec) {
                if (!ec) {
                    OnStreamEvent();
                } else {
                    DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "?? (%u) server::run: event_handle_->async_wait: ec.message=%s. \n", __LINE__, ec.message().c_str());
                }
            }
    );

    // Wait for all threads in the pool to exit.
    for (std::size_t i = 0; i < threads.size(); ++i)
        threads[i]->join();
    
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) server::run: out. \n", __LINE__);
}
    
void server::stop()
{
    handle_close_server();
}

void server::start_accept()
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) server(%#x)::start_accept: in. \n", __LINE__, this);
    
    new_connection_.reset(new connection(io_context_, connection_manager_, request_handler_, on_msg_, usr_arg_));

    acceptor_.async_accept(new_connection_->socket(),
                           std::bind(&server::handle_accept, shared_from_this(), std::placeholders::_1));
    
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) server::start_accept: out. \n", __LINE__);
}

void server::handle_accept(const std::error_code &e)
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) server::handle_accept: in. \n", __LINE__);

    try {
        // Check whether the server was stopped by a signal before this completion
        // handler had a chance to run.
        if (!acceptor_.is_open()) {
            DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) server::handle_accept: !acceptor_.is_open(). \n", __LINE__);
            return;
        }

        if (!e) {
            DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) server::handle_accept: connection_manager_.start(new_connection_=%#x). \n", __LINE__, new_connection_.get());
            connection_manager_.start(new_connection_);
        }
        else {
            DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) server::handle_accept: error=%s). \n", __LINE__, e.message().c_str());
        }
    }
    catch (std::exception &e) {
        DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) server::handle_accept: error=%s. \n", __LINE__, e.what());
        return;
    }

    start_accept();
    
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) server::handle_accept: out. \n", __LINE__);
}

void server::handle_stop_clients()
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) server::handle_stop_clients: connection_manager_.stop_all(). \n", __LINE__);
    connection_manager_.stop_all();
}

void server::handle_close_server()
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) server::handle_close_server: acceptor_.close(). \n", __LINE__);
    acceptor_.close();

    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) server::handle_close_server: connection_manager_.stop_all(). \n", __LINE__);
    connection_manager_.stop_all();

    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) server::handle_close_server: io_service_.stop(). \n", __LINE__);
#if 1
	io_context_.stop();
#else
	io_context_->reset();
	io_context_->stop();
	io_context_.reset();
#endif

    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) server::handle_close_server: out. \n", __LINE__);
}

void server::PushFrame(MEDIA_FRAME_FMT fmt, const unsigned char *frame_buff, int frame_len)
{
    net::tcp::server::FrameFactoryStorage frame;
	memset(&frame, 0, sizeof(net::tcp::server::FrameFactoryStorage));
	int nal_type = 0xff;

	frame.frame_buff = (unsigned char *)frame_buff;
	frame.frame_len = frame_len;
    frame.fmt = fmt;

    if (frame.fmt == MEDIA_FRAME_FMT_H264 ||
        frame.fmt == MEDIA_FRAME_FMT_H265)
    {
        frame.mt = FFS_MEDIA_TYPE_VIDEO;

        if (true == check_nal_type((unsigned char *)frame_buff, frame_len, &nal_type, NULL, NULL)) {
            if (7 == nal_type) {
                frame.encode_frame_type = net::tcp::server::FFS_ENCODE_FRAME_TYPE_I;

                {
                    std::lock_guard<std::mutex> lock(last_I_frm_lock_);
                    if (last_I_frm_mem_len_ <= 0 || last_I_frm_mem_len_ < frame_len) {
						if (last_I_frm_) {
							delete []last_I_frm_;
							last_I_frm_ = NULL;
						}

                        last_I_frm_mem_len_ = frame_len << 1;
                        last_I_frm_ = new unsigned char[last_I_frm_mem_len_];

                        DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG,
                                    "(%u) server::PushFrame: 7 == nal_type, reset last_I_frm_, last_I_frm_mem_len_=%u. \n",
                                    __LINE__, last_I_frm_mem_len_);
                    }

                    last_I_frm_len_ = frame_len;
                    memcpy(last_I_frm_, frame_buff, last_I_frm_len_);
                }

                //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) server::PushFrame: 7 == nal_type, get last_I_frm_, last_I_frm_len_=%u. \n", __LINE__, last_I_frm_len_);
            }
            else {
                frame.encode_frame_type = net::tcp::server::FFS_ENCODE_FRAME_TYPE_P;
            }
        }

        //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) server::PushFrame: kp_count_pfrm_=%d. \n", __LINE__, kp_count_pfrm_);
        kp_count_pfrm_++;
    }
    else {
        frame.mt = FFS_MEDIA_TYPE_AUDIO;
    }

	connection_manager_.PushFrame(&frame);
}

void GetSelBits(unsigned char **bits, unsigned int *len)
{
    static unsigned char h264_sel_bits[12] = {
            0x00, 0x00, 0x00, 0x01,
            0x06, 0x05, 0x05, 0x00,
            0x00, 0x00, 0x00, 0x00
    };

    if (bits) {
        *bits = h264_sel_bits;
    }

    if (len) {
        *len = 12;
    }
}

void server::OnStreamEvent()
{
    if (true == kp_frm_timeout_checker_.CalculStreamTimeout(kp_count_pfrm_, KEEP_PUSH_FRAME_INTVAL*1000)) {

        //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) OnStreamEvent: kp_count_pfrm_=%d, kp_frm_timeout_checker_ timeout.... \n", __LINE__, kp_count_pfrm_);

        {
            std::lock_guard<std::mutex> lock(last_I_frm_lock_);
            if (last_I_frm_) {
                DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) OnStreamEvent: kp_frm_timeout_checker_ timeout, PushVideoFrame last I, last_I_frm_len_=%u. \n", __LINE__, last_I_frm_len_);
                net::tcp::server::FrameFactoryStorage frame;
                memset(&frame, 0, sizeof(net::tcp::server::FrameFactoryStorage));

                frame.frame_buff = (unsigned char *)last_I_frm_;
                frame.frame_len = last_I_frm_len_;
                frame.encode_frame_type = FFSEncodeFrameType::FFS_ENCODE_FRAME_TYPE_I;
                frame.fmt = MEDIA_FRAME_FMT_H264;
                connection_manager_.PushFrame(&frame);
            }
        }
    }

    event_handle_->expires_after(std::chrono::seconds(EVENT_INTVAL));
    auto self(shared_from_this());
    event_handle_->async_wait(
            [this, self](const std::error_code &ec) {
                if (!ec) {
                    OnStreamEvent();
                }
                else {
                    DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "?? (%u) OnStreamEvent: event_handle_->async_wait: ec.message=%s. \n", __LINE__, ec.message().c_str());
                }
            }
    );
}

} // namespace server
} // namespace tcp
} // namespace net
