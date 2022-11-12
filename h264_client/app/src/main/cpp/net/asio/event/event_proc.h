#pragma once

#include <memory>
#include <thread>

#include <asio.hpp>
#ifdef __cplusplus
extern "C" {
#endif

namespace net {

class AsioEventService;

class ASIO_Event :	public std::enable_shared_from_this<ASIO_Event>
{
public:
	int type_;
};

typedef void (OnASIOEventProcT)(net::AsioEventService &aev_service);

class AsioEventService
  : public asio::io_context
{
	#define QUEUE_SIZE 10

public:

	explicit AsioEventService();
	virtual ~AsioEventService();

	void StartIoserviceThread();
	void StopIoserviceThread();

	void Run();

	//void CreateEventProc(std::function<OnASIOEventProcT> in);
	//void OnEventProc();
	//void PushEvent(std::shared_ptr<ASIO_Event> &e);
	//boost::shared_ptr<ASIO_Event> WaitEvent();

private:
 
	std::size_t thread_pool_size_;
	std::shared_ptr<std::thread> io_service_thread_;
	asio::signal_set signals_;

	//std::function<OnASIOEventProcT> on_event_proc_;
	//const void *usr_arg_;

	//bounded_semaphore_buffer<std::shared_ptr<ASIO_Event> > bounded_event_;
	//std::mutex bounded_frame_mutex_;
};

} // namespace net

#ifdef __cplusplus
}
#endif
