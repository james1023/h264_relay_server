
#include <cstring>
#include <iostream>
#include <sstream>
#include <functional>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#if defined(WIN32)

    #define WIN32_LEAN_AND_MEAN
    #include "windows.h"

    #if defined(QT_CORE_LIB)
    #include <qDebug>
    #endif

#elif defined(__ANDROID__)

    #include <android/log.h>

#else

    #include <stdio.h>
    #include <stdarg.h>

#endif

#include "base/debug-msg-c.h"

void debug_msg(unsigned int level, const char *out)
{
	//#define _CONSOLE
#if defined(_CONSOLE)
	std::cout << out << std::endl;
	return;
#endif

#if defined(WIN32)
    #if defined(_MSC_VER)
        #if defined(QT_CORE_LIB)
            qDebug(out);
        #else
            OutputDebugStringA(out);
        #endif
    #elif defined(__MINGW32__)
        fprintf(stdout, "%s", out);
    #endif
#elif defined(__ANDROID__)
	switch (level) {
	case JAM_DEBUG_LEVEL_DEBUG:
		__android_log_print(ANDROID_LOG_DEBUG, "JNIMsg", "%s", out);
		break;
	case JAM_DEBUG_LEVEL_INFO:
		__android_log_print(ANDROID_LOG_INFO, "JNIMsg", "%s", out);
		break;
	case JAM_DEBUG_LEVEL_WARN:
		__android_log_print(ANDROID_LOG_WARN, "JNIMsg", "%s", out);
		break;
	case JAM_DEBUG_LEVEL_ERROR:
		__android_log_print(ANDROID_LOG_ERROR, "JNIMsg", "%s", out);
		break;
	}
#elif __APPLE__
#include "TargetConditionals.h"
	//#if TARGET_OS_MAC // Other kinds of Mac OS
	//#elif TARGET_OS_IPHONE // iOS device
	//#elif TARGET_IPHONE_SIMULATOR // iOS Simulator
	//#else
	//#   error "Unknown Apple platform"
	//#endif

	std::cout << out << std::endl;
#else
	fprintf(stdout, "%s", out);
#endif
}

std::mutex debug_msg_mutex_;
char *debug_msg_buff = NULL;
void debug_msg_put(unsigned int level, const char *fmt, ...)
{
	const int debug_msg_buff_len = 4096;
	{
		//std::lock_guard<std::mutex> lock(debug_msg_mutex_);
		if (NULL == debug_msg_buff) {
			debug_msg_buff = new char[debug_msg_buff_len];
		}
	}
	
	if (level < DEBUG_LEVEL)
		return;

	va_list argptr;

	va_start(argptr, fmt);
	switch (level) {
	case JAM_DEBUG_LEVEL_DEBUG:
		sprintf(debug_msg_buff, "D/JDBG: ");
		break;
	case JAM_DEBUG_LEVEL_INFO:
		sprintf(debug_msg_buff, "I/JDBG: ");
		break;
	case JAM_DEBUG_LEVEL_WARN:
		sprintf(debug_msg_buff, "W/JDBG: ");
		break;
	case JAM_DEBUG_LEVEL_ERROR:
		sprintf(debug_msg_buff, "E/JDBG: ");
		break;
	}
	vsnprintf(debug_msg_buff+strlen(debug_msg_buff), debug_msg_buff_len-strlen(debug_msg_buff), fmt, argptr);
	debug_msg_buff[debug_msg_buff_len - 2] = '\n';
	debug_msg_buff[debug_msg_buff_len - 1] = 0;
	va_end(argptr);

	if (strlen(debug_msg_buff) > debug_msg_buff_len) {
		level = JAM_DEBUG_LEVEL_ERROR;
		sprintf(debug_msg_buff, "E/JDBG: (%u) %s !!! !!! !!! !!! !!! !!!", __LINE__, __FUNCTION__);
	}

	debug_msg(level, debug_msg_buff);
}