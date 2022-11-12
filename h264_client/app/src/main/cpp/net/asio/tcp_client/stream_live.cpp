
#include "stream_live.h"

#include "base/debug-msg-c.h"
#include "base/time-common.h"
#include "base/url/url_own.h"
#include "net/asio/tcp_client/IBaseStream.h"
#include "net/asio/tcp_client/CTCPStream.h"

StreamLive::StreamLive()
: tcp_stream_live_cond_exit_(false)
{
}

StreamLive::~StreamLive()
{

}

unsigned int StreamLive::CreateClient(const char *ip, TRANSPORT_PROTOCOL_TYPE trans_proto, const char *username, const char *password,
										OnFrameCb *on_frame, OnStatusCb *on_status, const void *usr_arg)
{
	int sid = 0;
	std::stringstream ss;

	do {
		net::StreamMessage strm_msg;

		strm_msg.set_username(username);
		strm_msg.set_password(password);

		ss.str("");
		if (TRANSPORT_PROTOCOL_RTSP_MULTICAST == trans_proto) {
			strm_msg.set_transport_protocol(strm_msg.TRANSPORT_PROTOCOL_RTSP_MULTICAST);
			ss << "rtsp://" << ip << ":20200/testStream";
		}
		else if (TRANSPORT_PROTOCOL_NOVO_TCP == trans_proto) {
			strm_msg.set_transport_protocol(strm_msg.TRANSPORT_PROTOCOL_NOVO_TCP);
			ss << "http://" << ip << ":20300";
		}
		else if (TRANSPORT_PROTOCOL_RTSP_TCP_PATH == trans_proto) {
			strm_msg.set_transport_protocol(strm_msg.TRANSPORT_PROTOCOL_RTSP_TCP_PATH);
			ss << ip;
		}
		else {
			strm_msg.set_transport_protocol(strm_msg.TRANSPORT_PROTOCOL_NOVO_TCP);
			ss << "http://" << ip << ":20300";

			// strm_msg.set_transport_protocol(strm_msg.TRANSPORT_PROTOCOL_MIRACAST);
			// ss << "http://" << ip << ":7236";
		}
		strm_msg.set_url(ss.str());

		std::shared_ptr<IBaseStream> strm;

		if (strm_msg.TRANSPORT_PROTOCOL_RTSP_MULTICAST == strm_msg.transport_protocol()) {
			//strm = std::shared_ptr<IBaseStream>(new CRtspStream(stream_manager_));
		}
		else if (strm_msg.TRANSPORT_PROTOCOL_NOVO_TCP == strm_msg.transport_protocol()) {
			strm = std::shared_ptr<IBaseStream>(new CTCPStream(stream_manager_));
		}
		else if (strm_msg.TRANSPORT_PROTOCOL_RTSP_TCP_PATH == strm_msg.transport_protocol()) {
			//strm = std::shared_ptr<IBaseStream>(new CRtspStream(stream_manager_));
		}
		else {
			strm = std::shared_ptr<IBaseStream>(new CTCPStream(stream_manager_));
			//strm = std::shared_ptr<IBaseStream>(new MiracastSinkStream(stream_manager_));
		}

		strm->set_stream_login_message(strm_msg);
		strm->set_callback(on_frame, on_status, usr_arg);

		sid = stream_manager_.Create(strm);
	} while (0);

	//DEBUG_TRACE_QUEUE(0, JAM_DEBUG_LEVEL_DEBUG, "sid=%d. ", sid);
    
    return sid;
}

bool StreamLive::StartClient(unsigned int sid)
{
	DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) StreamLive::StartClient, sid=%u. \n", __LINE__, sid);

	std::shared_ptr<IBaseStream> s;
	if (NULL != (s = stream_manager_.Get(sid))) {
		s->AsyncConnect();
	}
	else {
		DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "?(%u) StreamLive::StartClient, error, sid=%u. \n", __LINE__, sid);
		return false;
	}

	return true;
}

bool StreamLive::DestroyClient(unsigned int sid)
{
	DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) StreamLive::DestroyClient, sid=%u. \n", __LINE__, sid);

	std::shared_ptr<IBaseStream> s;
	if (NULL != (s = stream_manager_.Get(sid))) {
		s->set_notify_remove(true);
	}
	else {
		DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "?(%u) StreamLive::DestroyClient, error, sid=%u. \n", __LINE__, sid);
		return false;
	}

	return true;
}

bool StreamLive::DestroyAllClient()
{
	DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) StreamLive::DestroyAllClient. \n", __LINE__);

	stream_manager_.DestroyAll();

	return true;
}

bool StreamLive::SetPacketCB(unsigned int sid, OnPacketCb on_packet)
{
	DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) StreamLive::SetPacketCB, sid=%u. \n", __LINE__, sid);

	std::shared_ptr<IBaseStream> s;
	if (NULL != (s = stream_manager_.Get(sid))) {
		s->set_packet_cb(on_packet);
	}
	else {
		DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "?(%u) StreamLive::SetPacketCB, error, sid=%u. \n", __LINE__, sid);
		return false;
	}

	return true;
}

_INTERFACE_LIB_API unsigned int IStreamClient::CreateClient(const char *ip, TRANSPORT_PROTOCOL_TYPE trans_proto, const char *username, const char *password, OnFrameCb *on_frame, OnStatusCb *on_status, const void *usr_arg)
{
    return StreamLive::Instance().CreateClient(ip, trans_proto, username, password, on_frame, on_status, usr_arg);
}

_INTERFACE_LIB_API bool IStreamClient::StartClient(unsigned int sid)
{
	return StreamLive::Instance().StartClient(sid);
}

_INTERFACE_LIB_API bool IStreamClient::DestroyClient(unsigned int sid)
{
	return StreamLive::Instance().DestroyClient(sid);
}

_INTERFACE_LIB_API bool IStreamClient::DestroyAllClient()
{
	return StreamLive::Instance().DestroyAllClient();
}

_INTERFACE_LIB_API bool IStreamClient::SetPacketCB(unsigned int sid, OnPacketCb on_packet)
{
	return StreamLive::Instance().SetPacketCB(sid, on_packet);
}
