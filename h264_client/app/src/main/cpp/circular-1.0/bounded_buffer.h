
#pragma once

#include <deque>
#include <list>
#include <string>
#include <iostream>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include "circular-1.0/circular.h"
#include "base/debug-msg-c.h"
template <class T>
class bounded_buffer {
public:
    typedef circular_buffer<T> container_type;
    typedef typename container_type::size_type size_type;
    typedef typename container_type::value_type value_type;
    
    typedef void (OnPushNotifyCb)(const value_type *in_val, value_type *queue_val);
    typedef void (OnPopNotifyCb)(const value_type *queue_val, value_type *out_val, size_type un_read);
    
    explicit bounded_buffer(std::string name, size_type capacity) : name_(name), stop_(false), begin_(0), end_(0), m_unread(0), m_container(capacity) {
        for (unsigned long i = capacity; i > 0; --i) {
            m_container.push_back(value_type());
        }
    }
    
    void stop_queue() {

		DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) %s::stop_queue: 1. \n", __LINE__, name_.c_str());

		set_stop(true);

		DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) %s::stop_queue: 2. \n", __LINE__, name_.c_str());

		{
			std::unique_lock<std::mutex> lock(m_mutex);

			DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) %s::stop_queue: 3, m_unread=0, m_not_full.notify_one(). \n", __LINE__, name_.c_str());

			m_unread = 0;
		}

		m_not_full.notify_one();

		DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) %s::stop_queue: 4. \n", __LINE__, name_.c_str());

		{
			std::unique_lock<std::mutex> lock(m_mutex);

			DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) %s::stop_queue: 5, m_unread=1, m_not_empty.notify_one(). \n", __LINE__, name_.c_str());

			m_unread = 1;
		}

		m_not_empty.notify_one();

		DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) %s::stop_queue: exit. \n", __LINE__, name_.c_str());
    }
    
    bool push(const value_type *in_val, std::function<OnPushNotifyCb> on_push_notify) {
        bool res = true;
        
        //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) +++ ::push_queue: 1. \n", __LINE__);
        
        do {
            if (true == stop()) {
				DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) %s::push: exit. \n", __LINE__, name_.c_str());
                res = false;
                break;
            }

			//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) +++ ::push_queue: 2. \n", __LINE__);

			std::chrono::steady_clock::time_point push_all_ts_now_1 = std::chrono::steady_clock::now();
			int temp_uread = 0;
            
			{
                std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

				//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) +++ %s::push_queue: 3. \n", __LINE__, name_.c_str());

				std::unique_lock<std::mutex> lock(m_mutex);

				//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) +++ %s::push_queue: 4. \n", __LINE__, name_.c_str());

				m_not_full.wait(lock, std::bind(&bounded_buffer<value_type>::is_not_full, this));

				//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) +++ %s::push_queue: 5. \n", __LINE__, name_.c_str());

				if (true == stop()) {
					DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) %s::push: exit. \n", __LINE__, name_.c_str());
					res = false;
					break;
				}

				int push_lock_cost_ts_ = (int)(std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - now).count() * 1000);
				if (push_lock_cost_ts_ > 3) {
					//DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) bounded_packet_lock.test: push lock and wait=%d ms, unread=%u. \n", __LINE__, push_lock_cost_ts_, m_unread+1);
				}

				//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) +++ ::push_queue: 6. \n", __LINE__);

				on_push_notify(in_val, &m_container[end_]);
				end_ = (end_ + 1) % m_container.size();

				++m_unread;

                //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) +++ %s::push_queue: end_=%d, m_unread=%d. \n", __LINE__, name_.c_str(), end_, m_unread);

				temp_uread = m_unread;
			}

			//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) +++ ::push_queue: 7. \n", __LINE__);

            m_not_empty.notify_one();

			int push_all_ts = (int)(std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - push_all_ts_now_1).count() * 1000);
			if (push_all_ts > 3) {
				//DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) bounded_packet_lock.test: push all=%d ms, unread=%u. \n", __LINE__, push_all_ts, temp_uread);
			}
        } while(0);
        
        //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) +++ ::push_queue: out. \n", __LINE__);

        return res;
    }
    
    bool try_push(const value_type *in_val, std::function<OnPushNotifyCb> on_push_notify) {
        return try_push_for(in_val, on_push_notify, 0);
    }
    
    bool try_push_for(const value_type *in_val, std::function<OnPushNotifyCb> on_push_notify, int wait_time_ms) {
        bool res = true;
        
        do {
            if (true == stop()) {
				DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) bounded_buffer::try_push_for: stop. \n", __LINE__);
                res = false;
                break;
            }
            
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				if (true != m_not_full.wait_for(lock,
					std::chrono::milliseconds(wait_time_ms),
					std::bind(&bounded_buffer<value_type>::is_not_full, this))) {

					// timeout

					res = false;
					break;
				}

				if (true == stop()) {
					DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) bounded_buffer::try_push_for: exit. \n", __LINE__);
					res = false;
					break;
				}

				on_push_notify(in_val, &m_container[end_]);
				end_ = (end_ + 1) % m_container.size();
				++m_unread;
			}
            
            m_not_empty.notify_one();
        } while(0);
        
        return res;
    }
    
    bool pop(value_type *out_val, std::function<OnPopNotifyCb> on_pop_notify) {
        bool res = true;

        //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) --- ::pop_queue: 1. \n", __LINE__);

        do {
            if (true == stop()) {
				DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) %s::pop: exit. \n", __LINE__, name_.c_str());
                res = false;
                break;
            }

			//std::chrono::steady_clock::time_point pop_all_ts_now_1 = std::chrono::steady_clock::now();
			int temp_uread = 0;

			//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) --- %s::pop_queue: 2. \n", __LINE__, name_.c_str());

			{
				std::unique_lock<std::mutex> lock(m_mutex);

				//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) --- %s::pop_queue: 3. \n", __LINE__, name_.c_str());

				m_not_empty.wait(lock, std::bind(&bounded_buffer<value_type>::is_not_empty, this));

				//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) --- %s::pop_queue: 4. \n", __LINE__, name_.c_str());

				if (true == stop()) {
					DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) %s::pop: exit. \n", __LINE__, name_.c_str());
					res = false;
					break;
				}

				on_pop_notify(&m_container[begin_], out_val, m_unread-1);
				begin_ = (begin_ + 1) % m_container.size();
				--m_unread;

                //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) --- %s::pop_queue: begin_=%d, m_unread=%d. \n", __LINE__, name_.c_str(), begin_, m_unread);

				temp_uread = m_unread;
			}

			//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) --- ::pop_queue: 5. \n", __LINE__);

            m_not_full.notify_one();

			/*int pop_all_ts = (int)(std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - pop_all_ts_now_1).count() * 1000);
			if (pop_all_ts > 3) {
				DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) bounded_packet_lock.test: pop all=%d ms, unread=%u. \n", __LINE__, pop_all_ts, temp_uread);
			}*/
        } while(0);
        
        //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) --- ::pop_queue: out. \n", __LINE__);

        return res;
    }
    
    bool try_pop(value_type *out_val, std::function<OnPopNotifyCb> on_pop_notify) {
        return try_pop_for(out_val, on_pop_notify, 0);
    }
    
    bool try_pop_for(value_type *out_val, std::function<OnPopNotifyCb> on_pop_notify, int wait_time_ms) {
        bool res = true;
        
        do {
            if (true == stop()) {
				DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) %s::try_pop_for: stop. \n", __LINE__, name_.c_str());
                res = false;
                break;
            }
            
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				if (true != m_not_empty.wait_for(lock,
					std::chrono::milliseconds(wait_time_ms),
					std::bind(&bounded_buffer<value_type>::is_not_empty, this))) {

					// timeout

					lock.unlock();
					res = false;
					break;
				}

				if (true == stop()) {
					DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) %s::try_pop_for: exit. \n", __LINE__, name_.c_str());
					res = false;
					break;
				}

				on_pop_notify(&m_container[begin_], out_val, m_unread);
				begin_ = (begin_ + 1) % m_container.size();
				--m_unread;
			}
            
            m_not_full.notify_one();
        } while(0);
        
        return res;
    }

	void push_front(value_type &item) {
		std::unique_lock<std::mutex> lock(m_mutex);
		m_not_full.wait(lock, std::bind(&bounded_buffer<value_type>::is_not_full, this));

		m_container[end_] = item;
		end_ = (end_ + 1) % m_container.size();
		++m_unread;

		lock.unlock();
		m_not_empty.notify_one();
	}

	void pop_back(value_type &pItem) {
		std::unique_lock<std::mutex> lock(m_mutex);
		m_not_empty.wait(lock, std::bind(&bounded_buffer<value_type>::is_not_empty, this));

		pItem = m_container[begin_];
		begin_ = (begin_ + 1) % m_container.size();
		--m_unread;

		lock.unlock();
		m_not_full.notify_one();
	}
    
    size_type unread() const {
        return m_unread;
    }
    
	bool stop() const { return stop_; }
	void set_stop(bool in) { stop_ = in; }
    
protected:
    bounded_buffer(const bounded_buffer &);              // Disabled copy constructor
    bounded_buffer& operator = (const bounded_buffer &); // Disabled assign operator
    
    bool is_not_empty() const { return m_unread > 0; }
    bool is_not_full() const { return m_unread < m_container.capacity(); }

    std::string name_;
    bool stop_;
    size_type begin_;
    size_type end_;
    size_type m_unread;
    container_type m_container;
    std::mutex m_mutex;
    std::condition_variable m_not_empty;
    std::condition_variable m_not_full;
};
