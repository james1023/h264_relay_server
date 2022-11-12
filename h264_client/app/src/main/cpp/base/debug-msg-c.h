#pragma once

#define JAM_DEBUG_VERSION		"20210119"

typedef enum _JamDebugLevel {
	JAM_DEBUG_LEVEL_DEBUG = 1,
	JAM_DEBUG_LEVEL_INFO = 2,
	JAM_DEBUG_LEVEL_WARN = 3,
	JAM_DEBUG_LEVEL_ERROR = 4
} JamDebugLevel;

//#if defined(NDEBUG)
//#define DEBUG_LEVEL			JAM_DEBUG_LEVEL_INFO
//#else
#define DEBUG_LEVEL			JAM_DEBUG_LEVEL_DEBUG
//#endif

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus 

void debug_msg(unsigned int level, const char *out);
void debug_msg_put(unsigned int level, const char *fmt, ...);

//#define DEBUG_TRACE(LEVEL,(x)) do { if (DEBUG_OPEN) {debug_msg_put(LEVEL,(x));} } while (0)
#define DEBUG_TRACE			debug_msg_put

#ifdef __cplusplus
}
#endif // __cplusplus