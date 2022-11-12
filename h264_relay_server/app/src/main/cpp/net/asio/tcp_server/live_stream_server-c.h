
#ifndef LIVE_STREAM_SERVER_H
#define LIVE_STREAM_SERVER_H

// #pragma message(".lib")
// #pragma comment(lib, ".lib")
#ifndef _INTERFACE_LIB_API
#define _INTERFACE_LIB_API
#endif


typedef enum {
	LIVE_STATUS_CONNECT_STARTED = 0,			// stream has started.
	LIVE_STATUS_CONNECT_DESTROYED,				// stream has been destroyed.
} LIVE_STATUS;

typedef enum {
	MEDIA_FRAME_FMT_H264						= 0,
	MEDIA_FRAME_FMT_H265						= 1,
	MEDIA_FRAME_FMT_PCM						= 10,
	MEDIA_FRAME_FMT_AAC						= 11,
	MEDIA_FRAME_FMT_AC3						= 12,
} MEDIA_FRAME_FMT;

typedef void (*pOnStreamServerMsg)(const char *addr, LIVE_STATUS sts, const void *user_arg);
typedef void (OnStreamServerMsg)(const char *addr, LIVE_STATUS sts, const void *user_arg);
typedef struct {
  pOnStreamServerMsg on_msg;
} LiveCbs;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus 

_INTERFACE_LIB_API int CreateServer(const char *port, LiveCbs cbs, const void *usr_arg);
_INTERFACE_LIB_API void ReleaseServer();
_INTERFACE_LIB_API void PushFrame(MEDIA_FRAME_FMT fmt, const unsigned char *frame_buff, int frame_len);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // LIVE_STREAM_SERVER_H
