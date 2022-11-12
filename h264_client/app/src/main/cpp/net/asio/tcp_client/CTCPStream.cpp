
#include "CTCPStream.h"
#include "base/debug-msg-c.h"
#include "base/url/url_own.h"
#include "media/h264/H264Rtp.h"
#include "media/h264/parser.h"
#include "net/asio/tcp_client/stream_manager.h"

CTCPStream::CTCPStream(StreamManager &stream_manager):
strm_id_(0),
on_frame_(NULL),
on_packet_(NULL),
on_status_(NULL),
usr_arg_(NULL),
old_screen_format_(false),
now_frame_size_(0),
now_head_size_(0),
now_fmt_(MEDIA_FRAME_FMT_H264),
stream_manager_(stream_manager),
aev_service_(stream_manager.aev_service()),
stream_tcp_socket_(aev_service_),
has_first_frame_(false),
notify_start_live_(false),
stream_pre_frame_no_(0),
stream_frame_ok_count_(0),
notify_disconnect_count_(0),
timeout_frame_num_(0),
notify_disconnect_(false),
notify_remove_(false),
stream_buff_size_(0)
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) CTCPStream::CTCPStream. \n", __LINE__);
}

CTCPStream::~CTCPStream()
{
	DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) CTCPStream::~CTCPStream. \n", __LINE__);

	Disconnect();

	if (event_handle_) {
		event_handle_->cancel();
		event_handle_.reset();
	}
}

void CTCPStream::Connect() 
{
	DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) CTCPStream::Connect: in, no blocking, call AsyncConnect. \n", __LINE__);

	AsyncConnect();
}

void CTCPStream::AsyncConnect()
{
	DEBUG_TRACE(JAM_DEBUG_LEVEL_INFO, "(%u) CTCPStream::AsyncConnect: sid=%d, url=%s, in. \n", __LINE__, stream_id(), stream_login_message().url().c_str());

	if (nullptr == event_handle_) {
        DEBUG_TRACE(JAM_DEBUG_LEVEL_INFO, "(%u) CTCPStream::AsyncConnect: sid=%d, event_handle_.restart... \n", __LINE__, stream_id());
		event_handle_.reset(new asio::steady_timer(aev_service_));
		event_handle_->expires_after(std::chrono::seconds(1));
		event_handle_->async_wait(std::bind(&CTCPStream::StreamEventHandle, std::dynamic_pointer_cast<CTCPStream>(shared_from_this())));
	}

	Connect(stream_login_message().url().c_str(), stream_login_message().username().c_str(), stream_login_message().password().c_str());
}

BASE_ERR CTCPStream::Connect(const char *pchUrlName, const char *pchUserName, const char *pchPassword)
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "%s (%u) CTCPStream::Connect: pchUrlName=%s. \n", JAM_DEBUG_VERSION, __LINE__, pchUrlName);
    
	BASE_ERR err = BASE_OK;

	do 
	{
        try {
			base::OhUrl u(pchUrlName);
			asio::ip::tcp::endpoint ep(asio::ip::address::from_string(u.host()), u.port());

			stream_tcp_socket_.async_connect(ep,
				std::bind(&CTCPStream::HandleConnect, std::dynamic_pointer_cast<CTCPStream>(shared_from_this()),
					std::placeholders::_1));

			//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) CTCPStream::Connect: stream_tcp_socket_.connect in. \n", __LINE__);
			//asio::error_code ec;
			//stream_tcp_socket_.connect(ep, ec);
			//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) CTCPStream::Connect: stream_tcp_socket_.connect out, ec=%s. \n", __LINE__, ec.message().c_str());
        }
        catch (const std::exception &e) {
            err = BASE_IO_ERROR;
            DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "??? (%u) CTCPStream::Connect, error, e=%s, v=%d. \n", __LINE__, e.what(), err);
            break;
        }
	} while (0);

	return err;
}

void CTCPStream::HandleConnect(const std::error_code &ec)
{
	if (!ec) {
        DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN, ">>> (%u) CTCPStream::HandleConnect: sid=%d, in. \n", __LINE__, stream_id());

		stream_buff_size_ = 800 * 1024;

		if (nullptr == stream_buff_)
			stream_buff_ = std::shared_ptr<unsigned char>(new unsigned char[stream_buff_size_], [](unsigned char *p) { delete[] p; });

		asio::async_read(stream_tcp_socket_,
			asio::buffer(stream_buff_.get(), stream_buff_size_), asio::transfer_exactly(STREM_HEAD_FIRST_SIZE),
			std::bind(&CTCPStream::HandleStreamFirst, std::dynamic_pointer_cast<CTCPStream>(shared_from_this()),
				std::placeholders::_1,
				std::placeholders::_2));

        DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN, "<<< (%u) CTCPStream::HandleConnect: sid=%d, out. \n", __LINE__, stream_id());
	}
	else {
		DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN, "?? (%u) CTCPStream::HandleConnect: sid=%d, error, ec=%s. \n", __LINE__, stream_id(), ec.message().c_str());
	}
}

BASE_ERR CTCPStream::Disconnect()
{
	has_first_frame_ = (false);
	stream_pre_frame_no_ = (0);
	stream_frame_ok_count_ = (0);
	notify_start_live_ = (false);
	timeout_frame_num_ = (0);
	notify_disconnect_ = (false);

	std::error_code ignored_ec;
	stream_tcp_socket_.close(ignored_ec);

	return BASE_OK;
}

/*void CTCPStream::OnAsyncCallback(std::function<void(void)> on_callback_bind) {
	on_callback_bind();
}*/

void CTCPStream::CheckStreamEvent()
{
	if (true == timeout_stream_check_.CalculStreamTimeout(timeout_frame_num_, 10000)) {
		set_notify_disconnect(true);
		notify_disconnect_count_++;

		DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, ">>> (%u) CTCPStream::CheckStreamEvent: sid=%d, STREAM_CLIENT_LIVE_STATUS_CONNECT_INTERRUPTING, notify_disconnect_count_=%d, timeout_frame_num_=%u. \n", __LINE__, stream_id(), notify_disconnect_count_, timeout_frame_num_);
		aev_service_.post(std::bind(on_status(), stream_id(), STREAM_CLIENT_LIVE_STATUS_CONNECT_INTERRUPTING, usr_arg_));
        DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "<<< (%u) CTCPStream::CheckStreamEvent: sid=%d, STREAM_CLIENT_LIVE_STATUS_CONNECT_INTERRUPTING, out. \n", __LINE__, stream_id());
	}

	if (notify_disconnect_count_ >= 2) {
		set_notify_remove(true);
	}
}

void CTCPStream::StreamEventHandle()
{
	//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, ">>> (%u) CTCPStream::EventHandle: sid=%d, in. \n", __LINE__, stream_id());

	do {
		CheckStreamEvent();

		if (notify_remove()) {
			{
				std::lock_guard<std::recursive_mutex > lock(event_guard_);

				DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) CTCPStream::EventHandle, sid=%d, notify_remove(). \n", __LINE__, stream_id());

				Disconnect();
				stream_manager_.Destroy(shared_from_this());
			}

			DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, ">>> (%u) CTCPStream::EventHandle: sid=%d, call OnAsyncCallback, STREAM_CLIENT_LIVE_STATUS_CONNECT_DESTROYED. \n", __LINE__, stream_id());
			//DEBUG_RUN_TAG
			aev_service_.post(std::bind(on_status(), stream_id(), STREAM_CLIENT_LIVE_STATUS_CONNECT_DESTROYED, usr_arg_));
            DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "<<< (%u) CTCPStream::EventHandle: sid=%d, STREAM_CLIENT_LIVE_STATUS_CONNECT_DESTROYED out. \n", __LINE__, stream_id());
			//DEBUG_RUN_TAG

			break;
		}

		if (true == notify_disconnect()) 
		{
			set_notify_disconnect(false);

			{
				std::lock_guard<std::recursive_mutex > lock(event_guard_);

				DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) CTCPStream::EventHandle, sid=%d, notify_disconnect(), disconnect... \n", __LINE__, stream_id());

				Disconnect();

				DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) CTCPStream::EventHandle, sid=%d, notify_disconnect(), connect... \n", __LINE__, stream_id());

				AsyncConnect();

				DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) CTCPStream::EventHandle, sid=%d, notify_disconnect(), out... \n", __LINE__, stream_id());
			}
		}

		if (event_handle_) {
			event_handle_->expires_after(std::chrono::seconds(1));
			event_handle_->async_wait(std::bind(&CTCPStream::StreamEventHandle, std::dynamic_pointer_cast<CTCPStream>(shared_from_this())));
		}
	} while (0);

    //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "<<< (%u) CTCPStream::EventHandle: sid=%d, out. \n", __LINE__, stream_id());

	//DEBUG_RUN_TAG
}

void CTCPStream::HandleStreamFirst(const std::error_code &ec, std::size_t bytes_transferred)
{
	do {
		std::lock_guard<std::recursive_mutex > lock(event_guard_);

		if (notify_remove()) {
			break;
		}

		if (ec) {
			DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN, "?? (%u) CTCPStream::HandleStreamFirst: sid=%d, error, ec=%s. \n", __LINE__, stream_id(), ec.message().c_str());
			break;
		}

		if (true == losefrm_limit_checker_.CalculStreamTimeout(stream_frame_ok_count_, 5000)) {
			DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "?? (%u) CTCPStream::HandleStreamFirst: sid=%d, losefrm_limit_checker_ error. \n", __LINE__, stream_id());
			break;
		}

		if (stream_buff_.get()[4] == '$') {

			now_frame_size_ = 0;
			now_frame_size_ = stream_buff_.get()[0]; now_frame_size_ <<= 8;
			now_frame_size_ |= stream_buff_.get()[1]; now_frame_size_ <<= 8;
			now_frame_size_ |= stream_buff_.get()[2]; now_frame_size_ <<= 8;
			now_frame_size_ |= stream_buff_.get()[3];

			if (now_frame_size_ >= 800 * 1024) {
				DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "?? (%u) CTCPStream::HandleStreamFirst: sid=%d, frame_size error. \n", __LINE__, stream_id());
				break;
			}

			now_head_size_ = 0;
			now_head_size_ = stream_buff_.get()[6]; now_head_size_ <<= 8;
			now_head_size_ |= stream_buff_.get()[7];
		}
		else {
			stream_frame_ok_count_ = 0;

			//static net::CheckStreamRate head_checker;
			//if (true == head_checker.CalculStreamTimeout(false, 5000)) 
			{
				DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "?? (%u) CTCPStream::HandleStreamFirst: sid=%d, no $, bytes_transferred=%d. \n", __LINE__, stream_id(), bytes_transferred);
				break;
			}
		}

		if (now_head_size_ > 0) {
			try {
				auto self(shared_from_this());
				asio::async_read(stream_tcp_socket_,
					asio::buffer(stream_buff_.get(), stream_buff_size_), asio::transfer_exactly(now_head_size_),
					[this, self](std::error_code async_read_ec, std::size_t bytes_transferred) {
						HandleStreamHead(async_read_ec, bytes_transferred);
					});
			}
			catch (std::exception &try_send_ec) {
				DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "?? (%u) CRtspStream::HandleStreamFirst error, try_send_ec=%s. \n", __LINE__, try_send_ec.what());
			}
		}
		else {
			// old screen format.
			old_screen_format_ = true;
			now_head_size_ = 8;

			try {
				auto self(shared_from_this());
				asio::async_read(stream_tcp_socket_,
					asio::buffer(stream_buff_.get(), stream_buff_size_), asio::transfer_exactly(now_head_size_),
					[this, self](std::error_code async_read_ec, std::size_t bytes_transferred) {
						HandleStreamHead(async_read_ec, bytes_transferred);
					});
			}
			catch (std::exception &try_send_ec) {
				DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "?? (%u) CRtspStream::HandleStreamFirst error, try_send_ec=%s. \n", __LINE__, try_send_ec.what());
			}
		}
	} while (0);
}

void CTCPStream::HandleStreamHead(const std::error_code &ec, std::size_t bytes_transferred)
{
    do {
		std::lock_guard<std::recursive_mutex > lock(event_guard_);

		if (notify_remove()) {
			break;
		}

        if (ec) {
			DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN, "?? (%u) CTCPStream::HandleStreamHead: sid=%d, error, ec=%s. \n", __LINE__, stream_id(), ec.message().c_str());
            break;
        }

		if (old_screen_format_ == false) {
			now_fmt_ = (MEDIA_FRAME_FMT)stream_buff_.get()[4];
			//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) CTCPStream::HandleStreamHead: sid=%d, get $, fno=%d. \n", __LINE__, stream_id(), stream_buff_[5]);
			int now_fno = stream_buff_.get()[5];
			if (stream_pre_frame_no_ == 0) {
				stream_pre_frame_no_ = now_fno;
				stream_frame_ok_count_++;
			}
			else {
				if (now_fno == 0 || now_fno == stream_pre_frame_no_ + 1) {
					stream_frame_ok_count_++;
				}
				else {
					DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "?? (%u) CTCPStream::HandleStreamFirst: sid=%d, lose frame error, pre_fno_=%d, now_fno=%d, stream_frame_ok_count_=%d. \n", __LINE__, stream_id(), stream_pre_frame_no_, now_fno, stream_frame_ok_count_);
					stream_frame_ok_count_ = 0;
				}
				stream_pre_frame_no_ = now_fno;
			}
		}
		else {
			stream_frame_ok_count_++;
		}

		try {
			auto self(shared_from_this());
			asio::async_read(stream_tcp_socket_,
				asio::buffer(stream_buff_.get(), stream_buff_size_), asio::transfer_exactly(now_frame_size_),
					[this, self](std::error_code async_read_ec, std::size_t bytes_transferred) {
						HandleStreamBody(async_read_ec, bytes_transferred);
					}
				);
		}
		catch (std::exception &try_send_ec) {
			DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "?? (%u) CRtspStream::HandleStreamHead error, try_send_ec=%s. \n", __LINE__, try_send_ec.what());
		}
    } while(0);
}

void CTCPStream::HandleStreamBody(const std::error_code &ec, std::size_t bytes_transferred)
{
	STREAM_CLIENT_CB_PROPERTIES props;
	memset(&props, 0, sizeof(STREAM_CLIENT_CB_PROPERTIES));

	do {
		std::lock_guard<std::recursive_mutex> lock(event_guard_);

		if (notify_remove()) {
			break;
		}

		if (ec) {
			DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN,
						"### (%u) CTCPStream::HandleStreamBody: sid=%d, error, ec=%s. \n", __LINE__,
						stream_id(), ec.message().c_str());
			break;
		}

		// james: test
#if 0
		DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "### (%u) CTCPStream::HandleStreamBody: sid=%d, bytes_transferred=%u. \n", __LINE__, stream_id(), bytes_transferred);
#endif

		timeout_frame_num_++;
		notify_disconnect_count_ = 0;

		// 20180406 james: stream rate.
		static net::CheckStreamRate check_stream_rate;
#if 0
        if (true == check_stream_rate.CalculBitrate(bytes_transferred, 60))
        {
#else
		// james: test
		if (true == check_stream_rate.CalculBitrate(bytes_transferred, 3)) {
#endif

			DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG,
						"(%u) CTCPStream::HandleStreamBody: sid=%d, pkt_cb_kbitrate_=%d, pkt_cb_packet_=%d. \n",
						__LINE__, stream_id(), check_stream_rate.calbit_bitrate(),
						check_stream_rate.calbit_count());

			if (on_packet_) {
				SCPacketProperties packet_props;
				packet_props.kbitrate = check_stream_rate.calbit_bitrate();
				on_packet_(stream_id(), &packet_props, usr_arg_);
			}
		}

		//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) CRtspStream::handleStreamRead, sid=%d, bytes_transferred=%d. \n", __LINE__, stream_id(), bytes_transferred);

		if (false == has_first_frame()) {
			set_has_first_frame(true);

			DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG,
						">>> (%u) CTCPStream::HandleStreamBody: sid=%d, has_first_frame(), STREAM_CLIENT_LIVE_STATUS_CONNECT_STARTED, bytes_transferred=%d. \n",
						__LINE__, stream_id(), bytes_transferred);
			aev_service_.post(
					std::bind(on_status(), stream_id(), STREAM_CLIENT_LIVE_STATUS_CONNECT_STARTED,
							  usr_arg_));
			DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG,
						"<<< (%u) CTCPStream::HandleStreamBody: sid=%d, has_first_frame(), STREAM_CLIENT_LIVE_STATUS_CONNECT_STARTED out. \n",
						__LINE__, stream_id());

		}

		if (now_fmt_ == MEDIA_FRAME_FMT_H264 ||
			now_fmt_ == MEDIA_FRAME_FMT_H265) {

			props.e = props.CLIENT_CB_MEDIA_TYPE_VIDEO;
			props.u.video.fmt = now_fmt_;
			props.u.video.pBitstreamData = stream_buff_.get();
			props.u.video.dwBitstreamLength = bytes_transferred;
			props.u.video.wImageWidth = 1920;
			props.u.video.wImageHeight = 1080;
			props.u.video.frame_rate = 0;


			//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, ">>> (%u) CTCPStream::HandleStreamBody: sid=%d, GetH264SPSAndPPSData in, data=0x%.2x%.2x%.2x%.2x%.2x. \n",
			//        __LINE__, stream_id(), stream_buff_.get()[0], stream_buff_.get()[1], stream_buff_.get()[2], stream_buff_.get()[3], stream_buff_.get()[4]);
			unsigned char *sps_data = NULL, *pps_data = NULL;
			unsigned int sps_len = 0, pps_len = 0;
			if (true == GetH264SPSAndPPSData(stream_buff_.get(), bytes_transferred,
											 &sps_data, &sps_len,
											 &pps_data, &pps_len)) {
				props.u.video.sps_data = sps_data;
				props.u.video.sps_len = sps_len;
				props.u.video.pps_data = pps_data;
				props.u.video.pps_len = pps_len;

				props.u.video.bIFrame = 1;
			}
			//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "<<< (%u) CTCPStream::HandleStreamBody: sid=%d, GetH264SPSAndPPSData out. \n", __LINE__, stream_id());
		} else {
			props.e = props.CLIENT_CB_MEDIA_TYPE_AUDIO;
			props.u.audio.fmt = now_fmt_;
			props.u.audio.pBitstreamData = stream_buff_.get();
			props.u.audio.dwBitstreamLength = bytes_transferred;
		}

		//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "### (%u) CTCPStream::HandleStreamBody: sid=%d, wImageWidth=%d, bIFrame=%d. \n", __LINE__, stream_id(), props.u.video.wImageWidth, props.u.video.bIFrame);

		//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, ">>> (%u) CTCPStream::HandleStreamBody: sid=%d, on_frame. \n", __LINE__, stream_id());
		if (on_frame_)
			on_frame_(stream_id(), &props, usr_arg_);
		//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "<<< (%u) CTCPStream::HandleStreamBody: sid=%d, on_frame out. \n", __LINE__, stream_id());

		auto self(shared_from_this());
		asio::async_read(
				stream_tcp_socket_,
				asio::buffer(stream_buff_.get(), stream_buff_size_),
				asio::transfer_exactly(STREM_HEAD_FIRST_SIZE),
				[this, self](std::error_code async_read_ec, std::size_t bytes_transferred) {
					HandleStreamFirst(async_read_ec, bytes_transferred);
				}
		);
		//std::bind(&CTCPStream::HandleStreamHead, std::dynamic_pointer_cast<CTCPStream>(shared_from_this()),
		//	std::placeholders::_1,
		//	std::placeholders::_2));
	} while (0);
}