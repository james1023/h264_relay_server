
#include <vector>
#include <iostream>

#include "event_proc.h"
#include "base/time-common.h"
#include "base/debug-msg-c.h"

namespace net {

AsioEventService::AsioEventService()
 :  asio::io_context(),
	thread_pool_size_(4),
	signals_(*this)/*,
	bounded_event_(QUEUE_SIZE)*/
{
	
}

AsioEventService::~AsioEventService()
{
	StopIoserviceThread();
}

void AsioEventService::StartIoserviceThread()
{
	DEBUG_TRACE(JAM_DEBUG_LEVEL_INFO, "(%u) AsioEventService::StartIoserviceThread. \n", __LINE__);

	if (NULL == io_service_thread_) {

		DEBUG_TRACE(JAM_DEBUG_LEVEL_INFO, "(%u) AsioEventService::StartIoserviceThread: new thread pool. \n", __LINE__);

		io_service_thread_.reset(new std::thread([this] {
			signals_.add(SIGINT);
			signals_.add(SIGTERM);
#if defined(SIGQUIT)
			signals_.add(SIGQUIT);
#endif

			std::vector<std::shared_ptr<std::thread> > threads;

			signals_.async_wait(std::bind(&AsioEventService::StopIoserviceThread, this));

            std::size_t(asio::io_context::*io_context_run)() = &asio::io_context::run;
			for (std::size_t i = 0; i < thread_pool_size_; i++) {
				std::shared_ptr<std::thread> thread(new std::thread(std::bind(io_context_run, this)));
				threads.push_back(thread);
			}

			for (std::size_t i = 0; i < threads.size(); i++) {
				threads[i]->join();
			}
		}));
	}
}

void AsioEventService::StopIoserviceThread()
{
	DEBUG_TRACE(JAM_DEBUG_LEVEL_INFO, "(%u) AsioEventService::StopIoserviceThread in. \n", __LINE__);

	stop();

	if (io_service_thread_) {
		io_service_thread_->join();
	}

	DEBUG_TRACE(JAM_DEBUG_LEVEL_INFO, "(%u) AsioEventService::StopIoserviceThread out. \n", __LINE__);
}

void AsioEventService::Run()
{
	StartIoserviceThread();
}

/*void AsioEventService::OnEventProc()
{
	on_event_proc_(*this);
}*/

/*void AsioEventService::CreateEventProc(std::function<OnASIOEventProcT> in)
{
	on_event_proc_ = in;
	post(std::bind(&AsioEventService::OnEventProc, this));
}*/

/*void AsioEventService::PushEvent(std::shared_ptr<ASIO_Event> &e)
{
	bounded_event_.push_front(e);
}*/

/*std::shared_ptr<ASIO_Event> AsioEventService::WaitEvent()
{
	std::shared_ptr<ASIO_Event> e;
	bounded_event_.pop_back(e);

	return e;
}*/

} // namespace jam
