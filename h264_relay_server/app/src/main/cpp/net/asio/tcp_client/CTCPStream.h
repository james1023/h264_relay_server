#pragma once

#include <asio.hpp>

#include "net/stream/check_stream.h"
#include "media/h264/h264_stream.h"
#include "third_party/avtech/platform.h"
#include "third_party/avtech/stream-client-i.h"
#include "third_party/avtech/IBaseStream.h"
#include "third_party/avtech/CSocketAPI.h"
#include "third_party/avtech/TimeDate.h"
#include "third_party/avtech/common.h"
//#include "third_party/avtech/rtp_extension.pb.h"
#include "third_party/avtech/stream_live.pb.h"
#include "third_party/live555/live.2007.01.17/live/rtsp.h"
#include "third_party/live555/live.2007.01.17/live/rtsp_common.h"

class StreamManager;

class CNovoStream : public IBaseStream			
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

	CsSection m_Section;
	char m_chServerIp[128];
	DWORD m_dwServerPort;
	H264Tool m_H264Tool;
	int query_rtsp_fd_[10];
	BASESTREAM_PROTOCOL_MODE m_eStrmProtMode;
	BOOL m_force_multicast;
	BASESTREAM_FORMAT m_eImageStreamFormat;
	BASESTREAM_FORMAT m_eAudioStreamFormat;
	bool m_fIsGetVideo;
	bool m_fIsGetAudio;
	BOOL m_fIsGetMeta;
	char m_chProxyUrlName[128];
	int m_nSocketSize;
	BYTE *m_recv_packet_buffer;
	int m_recv_packet_max_len;
	int m_recv_packet_max_limit_len;
	BOOL m_is_status_stream;
    string pre_setparam_;

	CSocketAPI m_saRtspHttpRtcp;
	string m_strVendor;
    string model_name_;
	CQueryCounter m_qcTimestamp;
	CQueryCounter m_qcPushFrm;
	CQueryCounter m_qcPushPk;
	CQueryCounter m_callbits_qc;
	CQueryCounter m_recvpk_qc;
	CQueryCounter m_recvpk2_qc;

	// image.
	int m_nCurrImageFrameLen;
	BOOL m_fIsGetImageFrame;
	bool m_fgetFirstImageFrm;
	int m_nImageWidth;
	int m_nImageHeight;
	unsigned char m_quality;
	BOOL m_fIsIFrame;
	int m_nImageMode;
	int m_nImgPkCount;

	unsigned int timeout_frame_num_;
	net::CheckStreamRate timeout_stream_check_;

	net::CheckStreamRate losefrm_limit_checker_;

	CQueryCounter fr_count_framerate_qc_;
	int fr_curr_frames_;
	int fr_frame_rate_;

	DWORD m_dwTimeStamp;
	list<int> m_udp_pklen_queue; 
    DWORD img_total_cnt_;
	// audio.
	int m_nCurrAudioFrameLen;
	BOOL m_fIsGetAudioFrame;
	// meta.
	int m_nCurrMetaFrameLen;
	BOOL m_fIsGetMetaFrame;

	BOOL m_get_first_frm;
    unsigned int disp_i_frm_size_;
	int m_each_frm_ct;
    unsigned int pke_ct_by_frm_;
    unsigned int pke_size_;
    unsigned int com_each_loop_count_;
    unsigned int disp_com_each_loop_precount_;
	int m_nI_FrmCount;
	int m_nP_FrmCount;

	jam::AsioEventService &aev_service_;
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
	third_party::avtech::stream_live::StreamMessage strm_login_msg_;

public:
	CNovoStream(StreamManager &stream_manager);
	virtual ~CNovoStream();

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
		m_H264Tool.setUid(strm_id_);
	}
	unsigned int stream_id() {
		return strm_id_;
	}

    void add_img_total_cnt() {
        img_total_cnt_++;
    }
    int img_total_cnt() {
        return img_total_cnt_;
    }

	void SetSocketSize(__b_in int nSize);
	BASE_ERR Connect(const char *pchUrlName, const char *pchUserName, const char *pchPassword);
	BASE_ERR Disconnect();

	int GetVideoSocket();
	int GetAudioSocket();
	int GetMetaSocket();

	int GetSessionSocket();
	int GetVideoSessionSocket();
	int GetAudioSessionSocket();
	int GetMetaSessionSocket();
	bool IsGetVideoStreamOn() { return false; }
	bool IsGetAudioStreamOn() { return false; }
	bool IsGetMetaStreamOn() { return false; }
	bool IsReadySessionSocket();
	bool IsReadyVideoSessionSocket();

	void SetVideoProc();
	void SetAudioProc();
	void SetMetaProc();

	BASE_ERR GetPacketData(int Socket);
	BASE_ERR GetNextFrame();

	void HandleConnect(const std::error_code &e);
	void HandleStreamFirst(const std::error_code &e, std::size_t bytes_transferred);
    void HandleStreamHead(const std::error_code &e, std::size_t bytes_transferred);
	void HandleStreamBody(const std::error_code &e, std::size_t bytes_transferred);

	void CheckStreamEvent();
	void StreamEventHandle();
	//void OnAsyncCallback(std::function<void(void)> on_callback_bind);

	bool SetSessionData();
	bool SetVideoSessionData();
	bool SetAudioSessionData();
	bool SetMetaSessionData();

	void SetMulticast(__b_in bool on) {
		m_force_multicast = on;
	}

	bool has_first_frame() {
		return has_first_frame_;
	}
	void set_has_first_frame(bool in) {
		has_first_frame_ = in;
	}
    
    virtual bool notify_start_live() {
        return notify_start_live_;
    }
    void set_notify_start_live(bool in) {
        notify_start_live_ = in;
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

	int IsCodecFirstHeader(const BYTE *pbInBuff)
	{
		int start_code = _GetLongFromBytePtr(pbInBuff);
		if ((start_code & 0xFFFFFFFF) == 0xFFD8FFE0 || /*FFD8 Jpeg*/
			(start_code & 0xFFFFFFFF) == 0xFFD8FFDB || 
			(start_code & 0xFFFFFFFF) == 0xFFD8FFC4 ||
			(start_code & 0xFFFFFFFF) == 0xFFD8FFC0 ||
			(start_code & 0xFFFFFFFF) == 0xFFD8FFFE ||
			(start_code & 0xFFFFFF00) == 0x00000100 ||
			(start_code & 0xFFFFFFFF) == 0x00000001 || /*H264*/
			(start_code & 0xFFFFF0FF) == 0x0000E001 ||
			(start_code & 0xFFFFF0FF) == 0x0000D001 ||
			(start_code & 0xFFFFF0FF) == 0x0000C001)
		{
			return TRUE;
		}

		return FALSE;
	}

	int GetNextPacket(const BYTE **ppbNeedBuff, int *pnNeedLen);
    
    void add_com_each_loop_count() {
        com_each_loop_count_++;
    }
    unsigned int get_com_each_loop_count() {
        return com_each_loop_count_;
    }
    void set_com_each_loop_count(unsigned int in) {
        com_each_loop_count_ = in;
    }
	
	void addImgPkCount() {
		m_nImgPkCount++;
	}
	int getImgPkCount() {
		return m_nImgPkCount;
	}

	void count_frame_rate(int ms)
	{
		fr_curr_frames_++;
		if (fr_count_framerate_qc_.GetTimeintval(ms)) {
			fr_frame_rate_ = fr_curr_frames_ * 1000 / ms;

			fr_curr_frames_ = 0;
		}
	}
    
    // video data
    void set_sps_data(const unsigned char *data, int len) {
        /*RtspMediaData *pMediaData = m_pMediaData;
        
        SAFE_FREE(pMediaData->m_pVideoDataRec->sps_data);
        pMediaData->m_pVideoDataRec->sps_data = (unsigned char *)malloc(len);
        memcpy(pMediaData->m_pVideoDataRec->sps_data, (unsigned char *)data, len);
        pMediaData->m_pVideoDataRec->sps_len = len;*/
    }
    
    const unsigned char *sps_data() {
        //RtspMediaData *pMediaData = m_pMediaData;
        //return pMediaData->m_pVideoDataRec->sps_data;

		return NULL;
    }
    
    int sps_len() {
        /*RtspMediaData *pMediaData = m_pMediaData;
        return pMediaData->m_pVideoDataRec->sps_len;*/

		return 0;
    }
    
    void set_pps_data(const unsigned char *data, int len) {
        /*RtspMediaData *pMediaData = m_pMediaData;
        
        SAFE_FREE(pMediaData->m_pVideoDataRec->pps_data);
        pMediaData->m_pVideoDataRec->pps_data = (unsigned char *)malloc(len);
        memcpy(pMediaData->m_pVideoDataRec->pps_data, (unsigned char *)data, len);
        pMediaData->m_pVideoDataRec->pps_len = len;*/

		return;
    }
    
    const unsigned char *pps_data() {
        /*RtspMediaData *pMediaData = m_pMediaData;
        return pMediaData->m_pVideoDataRec->pps_data;*/

		return NULL;
    }
   
    int pps_len() {
        //RtspMediaData *pMediaData = m_pMediaData;
        //return pMediaData->m_pVideoDataRec->pps_len;

		return 0;
    }

	virtual third_party::avtech::stream_live::StreamMessage &stream_login_message() {
		return strm_login_msg_;
	}
	virtual void set_stream_login_message(third_party::avtech::stream_live::StreamMessage &in) {
		strm_login_msg_ = in;
	}
};
