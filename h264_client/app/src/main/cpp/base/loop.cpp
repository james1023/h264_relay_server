
#include <chrono>

#include "loop.h"
#include "base/debug-msg-c.h"
#include "base/time-common.h"

namespace base {

Loop::Loop()
: 	enable_stop_(false),
	timeval_ms_(33)
{
}

Loop::~Loop()
{
}

void Loop::Thread(std::function<OnCallCb> on_call, const void *usr_arg)
{
	double now_ts, next_ts;
	double val;
	while (1) {
		if (enable_stop()) {
			break;
		}

		now_ts = base::get_time();
		if (on_call) {
			on_call(usr_arg);
		}
		next_ts = base::get_time();
		val = (next_ts - now_ts)*1000;
		if (val > timeval())
			val = 0;
		else
			val = timeval() - val;

		if (val > 0)
			std::this_thread::sleep_for(std::chrono::milliseconds((int)val));
	}

    //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) Loop::Thread: out. \n", __LINE__);
}

void Loop::Start(std::function<OnCallCb> on_call, const void *usr_arg)
{
    //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) Loop::StartServer: in. \n", __LINE__);

    if (NULL == thread_)
        thread_.reset(new std::thread(std::bind(&Loop::Thread, this, on_call, usr_arg)));

    //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) Loop::Start: out. \n", __LINE__);
}

void Loop::Stop()
{
    //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) Loop::Stop: 1. \n", __LINE__);

	set_enable_stop(true);

    if (thread_) {
		thread_->join();
		thread_.reset();
    }

    //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) Loop::Stop: out. \n", __LINE__);
}

} // namespace base
