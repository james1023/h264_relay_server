#pragma once

#include <memory>
#include <functional>
#include <thread>

namespace base {

typedef void (OnCallCb)(const void *user_arg);

class Loop: public std::enable_shared_from_this<Loop>
{
private:
    std::shared_ptr<std::thread> thread_;

	bool enable_stop_;
	int timeval_ms_;

public:
	explicit Loop();
	virtual ~Loop();

	void Thread(std::function<OnCallCb> on_call, const void *usr_arg);

	void Start(std::function<OnCallCb> on_call, const void *usr_arg);
    void Stop();

	void set_timeval(int ms) {
		timeval_ms_ = ms;
	}
	int timeval() const {
		return timeval_ms_;
	}

	void set_enable_stop(bool in) {
		enable_stop_ = in;
	}
	bool enable_stop() const {
		return enable_stop_;
	}
};

} // namespace base {
