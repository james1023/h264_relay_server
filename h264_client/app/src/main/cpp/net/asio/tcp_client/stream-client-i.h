#ifndef __STREAM_CLIENT_I_H__
#define __STREAM_CLIENT_I_H__

#ifdef WIN32_LIB_EXPORT
#ifndef _INTERFACE_LIB_API
#define _INTERFACE_LIB_API   __declspec( dllexport )
#endif
#elif defined(WIN32_LIB_IMPORT)
#ifndef _INTERFACE_LIB_API
#define _INTERFACE_LIB_API   __declspec( dllimport )
#endif
#else
// #pragma message(".lib")
// #pragma comment(lib, ".lib")
#ifndef _INTERFACE_LIB_API
#define _INTERFACE_LIB_API
#endif
#endif

enum TRANSPORT_PROTOCOL_TYPE {
	TRANSPORT_PROTOCOL_RTSP_MULTICAST = 0,
	TRANSPORT_PROTOCOL_NOVO_TCP = 1,
	TRANSPORT_PROTOCOL_RTSP_TCP_PATH = 2,
};

enum BITSTREAM_FORMAT
{
	BITSTREAM_FORMAT_H264 = 0x00,
	BITSTREAM_FORMAT_MPEG4 = 0x01,
	BITSTREAM_FORMAT_JPEG = 0x02,
	BITSTREAM_FORMAT_ULAW = 0x03,
	BITSTREAM_FORMAT_H265 = 0x04,
};

enum MEDIA_FRAME_FMT {
	MEDIA_FRAME_FMT_H264 = 0,
	MEDIA_FRAME_FMT_H265 = 1,
	MEDIA_FRAME_FMT_PCM = 10,
	MEDIA_FRAME_FMT_AAC = 11,
	MEDIA_FRAME_FMT_AC3 = 12,
};

struct WIN_SYSTEM_TIME {
	unsigned short wYear;
	unsigned short wMonth;
	unsigned short wDayOfWeek;
	unsigned short wDay;
	unsigned short wHour;
	unsigned short wMinute;
	unsigned short wSecond;
	unsigned short wMilliseconds;
};

#define WIN_WAVE_FORMAT_PCM								1
struct WIN_WAVEFORMATEX
{
	unsigned short wFormatTag;									/* format type */
	unsigned short nChannels;										/* number of channels (i.e. mono, stereo...) */
	unsigned int nSamplesPerSec;									/* sample rate */
	unsigned int nAvgBytesPerSec;								/* for buffer estimation */
	unsigned short nBlockAlign;										/* block size of data */
	unsigned short wBitsPerSample;								/* number of bits per sample of mono data */
	unsigned short cbSize;											/* the count in bytes of the size of */
	/* extra information (after cbSize) */
};

struct WIN_TIMEVAL 
{
	long tv_sec;
	long tv_usec;
};

struct SCVideoProperties
{
    unsigned char               *pBitstreamData;
    unsigned int                dwBitstreamLength;
	MEDIA_FRAME_FMT				fmt;

	unsigned short				wImageWidth;
	unsigned short				wImageHeight;
    BITSTREAM_FORMAT            eBitstrmFmt;
    unsigned char               bServerTimeFlag;               // reserve
    WIN_TIMEVAL                 tClientTimeval;
    WIN_TIMEVAL                 tNVRTimeval;
    WIN_SYSTEM_TIME             stServerTime;
    unsigned int                dwTimeStamp;
	unsigned char				bImageMode;						// AVC_RTP_IMAGE_MODE_TYPE
	unsigned char				bIFrame;						// if it's I frame then bIFrame=1, else bIFrame=0.
	unsigned char				bQuality;						// 0~10
	int							frame_rate;
    
    const unsigned char         *sps_data;
    int                         sps_len;
    const unsigned char         *pps_data;
    int                         pps_len;
};

struct SCAudioProperties
{
    unsigned char               *pBitstreamData;
    unsigned int                dwBitstreamLength;
	MEDIA_FRAME_FMT				fmt;

	WIN_WAVEFORMATEX	        winWavefmt;
    unsigned int                dwTimeStamp;
};

struct STREAM_CLIENT_CB_PROPERTIES
{
	enum {
		CLIENT_CB_MEDIA_TYPE_VIDEO,
		CLIENT_CB_MEDIA_TYPE_AUDIO,
	} e;

	union {
		SCVideoProperties video;
		SCAudioProperties audio;
	} u;
};

enum STREAM_CLIENT_LIVE_STATUS {
	STREAM_CLIENT_LIVE_STATUS_CONNECT_STARTED = 0,				// stream has started.
	STREAM_CLIENT_LIVE_STATUS_CONNECT_INTERRUPTING,				// stream is interrupting, notify callback each 5 sec.
	STREAM_CLIENT_LIVE_STATUS_CONNECT_DESTROYED,				// stream has been destroyed.
};

struct SCPacketProperties
{
	int						kbitrate;
};

// Interface.
typedef void OnFrameCb(unsigned int sid, STREAM_CLIENT_CB_PROPERTIES *client_cb_props, const void *usr_arg);
typedef void OnStatusCb(unsigned int sid, STREAM_CLIENT_LIVE_STATUS status, const void *usr_arg);

typedef void OnPacketCb(unsigned int sid, SCPacketProperties *pkt_props, const void *usr_arg);

class _INTERFACE_LIB_API IStreamClient
{
public:
	/**
	* create a stream client
	*
	* @param ip:						server ip. ex: 192.168.1.1
	* @param trans_proto:				TRANSPORT_PROTOCOL_TYPE. ex: TRANSPORT_PROTOCOL_RTP_MULTICAST
	* @param on_frame:					stream callback.
	* @param on_status:					status callback.
	* @return sid:						stream id.
	*
	*/
	static unsigned int CreateClient(const char *ip, TRANSPORT_PROTOCOL_TYPE trans_proto, const char *username, const char *password,
		OnFrameCb on_frame, OnStatusCb on_status, const void *usr_arg);

	/**
	* start a stream client
	*
	* @param sid:						stream id.
	*
	*/
	static bool StartClient(unsigned int sid);
	
	/**
	* destroy a stream client
	*
	* @param sid:						stream id.
	*
	*/
	static bool DestroyClient(unsigned int sid);

	/**
	* destroy all stream clients
	*/
	static bool DestroyAllClient();

	/**
	* register packet callback
	*
	* @param sid:						stream id.
	* @param on_packet:					packet callback.
	*
	*/
	static bool SetPacketCB(unsigned int sid, OnPacketCb on_packet);
};

#endif	// __STREAM_CLIENT_I_H__
