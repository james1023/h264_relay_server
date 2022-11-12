#pragma once

#include <string>
#include <chrono>

#include "base/time-common.h"
#include "base/debug-msg-c.h"

#ifdef __cplusplus
namespace net {
extern "C" {
#endif

static inline void check_stream_rate(int packet_size)
{
    static int io_count = 0;
    static int pk_accu_size = 0;
    static double pre_ts_ = 0;
    static double pre_diff_ts_ = 0;

    static int delay_count = 0;
    static int delay_diff_accu_ms = 0;

    static int acce_count = 0;
    static int acce_diff_accu_ms = 0;

    double now = 0;
    int diff = 0;

    io_count ++;
    pk_accu_size += packet_size;

    if (0 == pre_ts_) {
        pre_ts_ = base::get_time();
        pre_diff_ts_ = base::get_time();
    }
    else {
        now = base::get_time();

        diff = (now - pre_diff_ts_)*1000;
        if (diff >= 40) {
            delay_count ++;
            delay_diff_accu_ms += (diff-30);
        }
        else if (diff <= 20) {
            acce_count ++;
            acce_diff_accu_ms += (30-diff);
        }
        pre_diff_ts_ = base::get_time();

        if ((now-pre_ts_)*1000 >= 1000) {
            DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG,
                        "(%u) check_stream_rate: io_count=%d, pk_acc_size=%d kbps, delay_count(>=40)=%d => accu_diff=%d ms, acce_count(<=20)=%d => accu_diff=%d ms. \n",
                        __LINE__, io_count, pk_accu_size * 8 / 1024, delay_count, delay_diff_accu_ms, acce_count, acce_diff_accu_ms);
            pre_ts_ = now;

            io_count = 0;
            pk_accu_size = 0;

            delay_count = 0;
            delay_diff_accu_ms = 0;

            acce_count = 0;
            acce_diff_accu_ms = 0;
        }
    }
}

class CheckStreamRate {
public:
	CheckStreamRate()
	 :	io_count_(0), pk_accu_size_(0), pre_ts_(0), pre_diff_ts_(0),
		delay_count_(0), delay_diff_accu_ms_(0), 
		acce_count_(0), acce_diff_accu_ms_(0),
		calbit_pre_ts_(0), calbit_pk_accu_size_(0), calbit_bitrate_(0), calbit_count_(0)
	{
		
	}

	inline void PrintStreamRate(std::string identity, int packet_size)
	{
		double now = 0;
		int diff = 0;
		const unsigned sec = 60;

		io_count_++;
		pk_accu_size_ += packet_size;

		if (0 == pre_ts_) {
			pre_ts_ = base::get_time();
			pre_diff_ts_ = base::get_time();
		}
		else {
			now = base::get_time();

			diff = (now - pre_diff_ts_) * 1000;
			if (diff >= 40) {
				delay_count_++;
				delay_diff_accu_ms_ += (diff - 30);
			}
			else if (diff <= 20) {
				acce_count_++;
				acce_diff_accu_ms_ += (30 - diff);
			}
			pre_diff_ts_ = base::get_time();

			if ((now - pre_ts_) * 1000 >= sec*1000) {
				DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG,
					"(%u) [CheckStreamRate] [%s], io_count=%d, pk_acc_size=%d kbps, delay_count(>=40)=%d => accu_diff=%d ms, acce_count(<=20)=%d => accu_diff=%d ms. \n",
					__LINE__, identity.c_str(), io_count_ / sec, pk_accu_size_ * 8 / 1024 / sec, delay_count_ / sec, delay_diff_accu_ms_ / sec, acce_count_ / sec, acce_diff_accu_ms_ / sec);
				pre_ts_ = now;

				io_count_ = 0;
				pk_accu_size_ = 0;

				delay_count_ = 0;
				delay_diff_accu_ms_ = 0;

				acce_count_ = 0;
				acce_diff_accu_ms_ = 0;
			}
		}
	}

	inline bool CalculBitrate(int packet_size, int each_sec)
	{
		double now = 0;
		
		if (0 == calbit_pre_ts_) {
			calbit_pre_ts_ = base::get_time();

			calbit_pk_accu_size_ = packet_size;
			calbit_bitrate_ = 0;
			calbit_count_ = 1;
		}
		else {
			now = base::get_time();

			calbit_pk_accu_size_ += packet_size;
			calbit_count_++;

			if ((now - calbit_pre_ts_) * 1000 >= each_sec*1000) {
				calbit_bitrate_ = calbit_pk_accu_size_ * 8 / 1024 / each_sec;
				calbit_count_ = calbit_count_ / each_sec;

				calbit_pre_ts_ = 0;

				return true;
			}
		}

		return false;
	}

	inline bool CalculStreamTimeout(bool is_get_data, int timeout_ms)
	{
		std::chrono::steady_clock::time_point now;

		if (0 == getdata_pre_ts_.time_since_epoch().count()) {
			getdata_pre_ts_ = std::chrono::steady_clock::now();
		}
		else {
			if (false == is_get_data) {
				now = std::chrono::steady_clock::now();
			}
			else {
				getdata_pre_ts_ = std::chrono::steady_clock::now();
				now = getdata_pre_ts_;
			}

			std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(now - getdata_pre_ts_);
			if (time_span.count() * 1000 >= timeout_ms) {
				getdata_pre_ts_ = std::chrono::steady_clock::now();
				return true;
			}
		}

		return false;
	}

	inline bool CalculStreamTimeout(unsigned int frm_now_count, int timeout_ms)
	{
		std::chrono::steady_clock::time_point now;

		if (0 == frm_pre_ts_.time_since_epoch().count()) {
			frm_pre_ts_ = std::chrono::steady_clock::now();
		}
		else {
			if (frm_now_count == frm_pre_count_) {
				now = std::chrono::steady_clock::now();
			}
			else {
				frm_pre_ts_ = std::chrono::steady_clock::now();
				now = frm_pre_ts_;

				frm_pre_count_ = frm_now_count;
			}

			std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(now - frm_pre_ts_);
			if (time_span.count() * 1000 >= timeout_ms) {
				frm_pre_ts_ = std::chrono::steady_clock::now();
				frm_pre_count_ = frm_now_count;
				return true;
			}
		}

		return false;
	}

	void resetTimeout() {
		check_pre_ts_ = std::chrono::steady_clock::now();
	}

	inline bool CheckTimeout(int timeout_ms)
	{
		std::chrono::steady_clock::time_point now;

		if (0 == check_pre_ts_.time_since_epoch().count()) {
			check_pre_ts_ = std::chrono::steady_clock::now();
		}
		else {
			now = std::chrono::steady_clock::now();
			
			std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(now - check_pre_ts_);
			if (time_span.count() * 1000 >= timeout_ms) {
				check_pre_ts_ = std::chrono::steady_clock::now();
				return true;
			}
		}

		return false;
	}

	int calbit_bitrate() const {
		return calbit_bitrate_;
	}

	int calbit_count() const {
		return calbit_count_;
	}
	
private:
	int io_count_;
	int pk_accu_size_;
	double pre_ts_;
	double pre_diff_ts_;

	int delay_count_;
	int delay_diff_accu_ms_;

	int acce_count_;
	int acce_diff_accu_ms_;

	int calbit_pk_accu_size_;
	int calbit_bitrate_;
	int calbit_count_;
	double calbit_pre_ts_;

	std::chrono::steady_clock::time_point getdata_pre_ts_;

	std::chrono::steady_clock::time_point frm_pre_ts_;
	unsigned int frm_pre_count_;

	std::chrono::steady_clock::time_point check_pre_ts_;
};

#ifdef __cplusplus
}  // extern "C"
}  // namespace net
#endif
