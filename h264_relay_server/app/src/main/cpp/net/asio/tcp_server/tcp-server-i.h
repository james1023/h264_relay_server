#pragma once

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

#include <functional>
  
//
// Interface.
//

namespace net {

class _INTERFACE_LIB_API ITcpServer
{
public:
	enum STREAM_SERVER_LIVE_STATUS {
		STREAM_SERVER_LIVE_STATUS_CONNECT_STARTED = 0,				// stream has started.
		STREAM_SERVER_LIVE_STATUS_CONNECT_DESTROYED,				// stream has been destroyed.
	};

	enum MEDIA_FRAME_FMT {
		MEDIA_FRAME_FMT_H264						= 0,
		MEDIA_FRAME_FMT_H265						= 1,
		MEDIA_FRAME_FMT_PCM							= 10,
		MEDIA_FRAME_FMT_AAC							= 11,
		MEDIA_FRAME_FMT_AC3							= 12,
	};

	typedef void (OnServerMsg)(std::string addr, STREAM_SERVER_LIVE_STATUS sts, const void *user_arg);

public:
	static bool CreateServer(std::string port, std::function<ITcpServer::OnServerMsg> on_msg, const void *usr_arg);
	static void ReleaseServer();
	static void PushFrame(MEDIA_FRAME_FMT fmt, const unsigned char *frame_buff, int frame_len);
};

} // namespace net