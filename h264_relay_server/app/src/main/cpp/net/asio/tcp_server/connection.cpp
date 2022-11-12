
#include <vector>
#include <iostream>

#include "base/time-common.h"
#include "base/debug-msg-c.h"
#include "net/check_stream.h"
#include "connection_manager.hpp"
#include "connection.hpp"
#include "request_handler.hpp"
		
namespace net {
namespace tcp {
namespace server {

connection::connection(asio::io_context &io_context,
                       connection_manager &manager, request_handler &handler,
                       std::function<OnStreamServerMsg> on_msg, const void *usr_arg)
  : io_context_(io_context),
    socket_(io_context),
    connection_manager_(manager),
    async_stream_(NULL),
    async_stream_mem_len_(0),
    request_handler_(handler),
	on_msg_(on_msg),
	usr_arg_(usr_arg),
	bounded_frame_("net::tcp::server::connection", QUEUE_SIZE),
    has_first_idr_(false),
    old_frame_no_(0),
    video_frame_no_(0),
	notify_lose_frame_(false)
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) net::tcp::server::connection: in, this=%#x. \n", __LINE__, this);

    //buffer_.reset(new std::vector<unsigned char>(70*1024, 0));
}

connection::~connection() {
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) ~net::tcp::server::connection: in, this=%#x. \n", __LINE__, this);

    if (async_stream_) {
        free(async_stream_);
        async_stream_ = NULL;
    }
}

asio::ip::tcp::socket& connection::socket()
{
    return socket_;
}

void connection::start()
{
	try {
        local_ip_ = socket().remote_endpoint().address().to_string();
        local_port_ = socket().remote_endpoint().port();

        DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) net::tcp::server::connection::start: set_option no_delay!!!!!!. \n", __LINE__);
		// 20201021 james: add no_delay option.
		const asio::ip::tcp::no_delay nodelay_option(true); 
		socket().set_option(nodelay_option);

        if (nullptr != on_msg_) {
            std::stringstream ss;
            ss << local_ip_ << ":" << local_port_;
            on_msg_(ss.str().c_str(), LIVE_STATUS_CONNECT_STARTED, usr_arg_);
        }
	}
    catch (std::exception &e) {
        DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) net::tcp::server::connection::start: error=%s. \n", __LINE__, e.what());
        return;
    }

    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) net::tcp::server::connection::start: PopFrame(). \n", __LINE__);
    //auto self(shared_from_this());
    //io_context_.post([this, self]() {
        PopFrame();
    //});
}
    
void connection::PushFrame(const FrameFactoryStorage *frame)
{
    if (true == video_fame_no_limit_checker_.CalculStreamTimeout((unsigned int)video_frame_no_, 10000)) {
        DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "\n\n(%u) connection(%#x)::PushFrame: video_fame_no_limit_checker_, stop(). \n\n", __LINE__, this);

        auto self(shared_from_this());
        io_context_.post([this, self]() {
            connection_manager_.stop(shared_from_this());

            if (nullptr != on_msg_) {
                std::stringstream ss;
                ss << local_ip_ << ":" << local_port_;
                on_msg_(ss.str().c_str(), LIVE_STATUS_CONNECT_DESTROYED, usr_arg_);
            }
        });
    }

    if (frame->mt == FFS_MEDIA_TYPE_VIDEO) {
        if (false == has_first_idr()) {
            if (net::tcp::server::FFS_ENCODE_FRAME_TYPE_I == frame->encode_frame_type) {
                set_has_first_idr(true);
            }
            else {
                DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) connection::PushFrame: no has_first_idr!!!. \n", __LINE__);
                return;
            }
        }

        if (notify_lose_frame()) {
            if (net::tcp::server::FFS_ENCODE_FRAME_TYPE_I != frame->encode_frame_type) {
                //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) net: connection::PushFrame, lose IDR!!!. \n", __LINE__);
                return;
            }
        }

        if (false == bounded_frame_.try_push(frame, std::bind(&connection::OnPushNotify, shared_from_this(), std::placeholders::_1, std::placeholders::_2))) {
            // lose IDR procedure.
            if (net::tcp::server::FFS_ENCODE_FRAME_TYPE_I == frame->encode_frame_type) {
                DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN, "(%u) connection(%#x)::PushFrame: bounded_frame_.try_push error, lose I, frame_len=%d. \n", __LINE__, this, frame->frame_len);
                set_notify_lose_frame(true);
            }
        }
        else {
            set_notify_lose_frame(false);
        }
    }
    else {
        if (false == bounded_frame_.try_push(frame, std::bind(&connection::OnPushNotify, shared_from_this(), std::placeholders::_1, std::placeholders::_2))) {
            DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN, "(%u) connection(%#x)::PushFrame: bounded_frame_.try_push error, lose audio, frame_len=%d. \n", __LINE__, this, frame->frame_len);
        }
    }

}

void connection::OnPushNotify(const FrameFactoryStorage *in_val, FrameFactoryStorage *queue_val)
{
    if (NULL == queue_val->frame_buff || queue_val->frame_mem_len <= in_val->frame_len)
    {
        if (queue_val->frame_buff) {
            free(queue_val->frame_buff);
            queue_val->frame_buff = NULL;
        }
        
        queue_val->frame_mem_len = in_val->frame_len << 1;
        queue_val->frame_buff = (unsigned char *)malloc(queue_val->frame_mem_len);
    }

    queue_val->frame_len = in_val->frame_len;
    queue_val->mt = in_val->mt;
    queue_val->fmt = in_val->fmt;

    queue_val->encode_frame_type = in_val->encode_frame_type;

	if (queue_val->frame_mem_len <= queue_val->frame_len) {
		DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) tcp::connection::OnPushNotify: error, queue_val->frame_mem_len=%d, queue_val->frame_len=%d. \n",
			__LINE__, queue_val->frame_mem_len, queue_val->frame_len);
		return;
	}
    
    memcpy(queue_val->frame_buff, in_val->frame_buff, queue_val->frame_len);
}

void connection::PopFrame() {
    //DEBUG_TRACE_QUEUE(0, JAM_DEBUG_LEVEL_DEBUG, "bounded_frame_.unread()=%d. ", bounded_frame_.unread());
    bounded_frame_.pop(NULL, std::bind(&connection::OnPopNotify, shared_from_this(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void connection::OnPopNotify(const FrameFactoryStorage *queue_val, FrameFactoryStorage *out_val, int un_read)
{
    //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) tcp::connection(%#x)::OnPopNotify, no=%d, frame_len=%d, async_stream_mem_len_=%d, un_read=%d. \n", 
	//	__LINE__, this, queue_val->no, queue_val->frame_len, async_stream_mem_len_, un_read);
    
    if (NULL == async_stream_ || async_stream_mem_len_ < (queue_val->frame_mem_len+RESERVE_HEAD_SIZE)) {
        if (async_stream_) {
            free(async_stream_);
            async_stream_ = NULL;
        }
        async_stream_mem_len_ = queue_val->frame_mem_len+RESERVE_HEAD_SIZE;
        async_stream_ = (unsigned char *)malloc(async_stream_mem_len_);
    }

    int step = 0;
    async_stream_[step] = (queue_val->frame_len >> 24) & 0xFF; step++;
    async_stream_[step] = (queue_val->frame_len >> 16) & 0xFF; step++;
    async_stream_[step] = (queue_val->frame_len >> 8) & 0xFF; step++;
    async_stream_[step] = queue_val->frame_len & 0xFF; step++;

    // flag
    async_stream_[step] = '$'; step++;
    int sno_offset = step;
    async_stream_[step] = 0; step++;
    // head size
    int old_head_size = 8;
    async_stream_[step] = (old_head_size>>8) & 0xFF; step++;
    async_stream_[step] = old_head_size & 0xFF; step++;


    //
    // head
    //
    // timestamp
    async_stream_[step] = 0; step++;
    async_stream_[step] = 0; step++;
    async_stream_[step] = 0; step++;
    async_stream_[step] = 0; step++;

    // media
    async_stream_[step] = queue_val->fmt; step++;
    if (queue_val->mt == FFS_MEDIA_TYPE_VIDEO) {
        async_stream_[step] = video_frame_no_++;
    }
    else {
        async_stream_[step] = 0;
    }
    step++;
    async_stream_[step] = 0; step++;
    async_stream_[step] = 0; step++;

    memcpy(async_stream_ + step, queue_val->frame_buff, queue_val->frame_len);

    {
        std::lock_guard<std::mutex> lock(old_frame_no_mtx);

        //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) tcp::connection(%#x)::OnPopNotify, fmt=%u, frame_len=%d, old_frame_no_=%u, video_frame_no_=%u, un_read=%d. \n",
        //            __LINE__, this, queue_val->fmt, queue_val->frame_len, old_frame_no_, video_frame_no_, un_read);

        async_stream_[sno_offset] = old_frame_no_++;
        asio::async_write(socket(), asio::buffer(async_stream_, queue_val->frame_len+step),
                          asio::transfer_exactly(queue_val->frame_len+step),
                          std::bind(&connection::handle_data, shared_from_this(),
                                    std::placeholders::_1, std::placeholders::_2));
    }

	//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) tcp::connection(%#x)::OnPopNotify, no=%d, out. \n", __LINE__, this, queue_val->no);
}

void connection::stop()
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) net::tcp::server::connection::stop: in. \n", __LINE__);

    bounded_frame_.stop_queue();
    socket().close();
}

void connection::handle_data(const std::error_code& e, std::size_t bytes_transferred)
{
	if (!e) {
		if (bytes_transferred > 0) {
			// 20180410 james: unit test for stream_rate
			static net::CheckStreamRate check_stream_rate;
			check_stream_rate.PrintStreamRate("net::tcp::server::connection::connection::handle_data", bytes_transferred);

			PopFrame();
		}
		else {
			DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "?? (%u) net::tcp::server::connection::handle_data: bytes_transferred<= 0. \n", __LINE__);
			connection_manager_.stop(shared_from_this());

            if (nullptr != on_msg_) {
                std::stringstream ss;
                ss << local_ip_ << ":" << local_port_;
                on_msg_(ss.str().c_str(), LIVE_STATUS_CONNECT_DESTROYED, usr_arg_);
            }
		}
	}
    else if (e != asio::error::operation_aborted) {
		DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "?? (%u) net::tcp::server::connection::handle_data: error, e!=asio::error::operation_aborted. \n", __LINE__);
        connection_manager_.stop(shared_from_this());

        if (nullptr != on_msg_) {
            std::stringstream ss;
            ss << local_ip_ << ":" << local_port_;
            on_msg_(ss.str().c_str(), LIVE_STATUS_CONNECT_DESTROYED, usr_arg_);
        }
    }
}

} // namespace server
} // namespace tcp
} // namespace net
