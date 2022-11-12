
#include <algorithm>

#include "base/debug-msg-c.h"
#include "net/asio/tcp_client/IBaseStream.h"
#include "net/asio/tcp_client/stream_manager.h"

StreamManager::StreamManager()
 :	saving_client_sid_(0)
{

}

StreamManager::~StreamManager()
{

}

int StreamManager::Create(std::shared_ptr<IBaseStream> s)
{
	aev_service_.Run();

	DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "<<< (%u) StreamManager::Create: in. \n", __LINE__);

	std::lock_guard<std::mutex> lock(stream_guard_);
    if (streams_.size() >= 4) {
        DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) StreamManager::Create: connections_.size()=%u, erase. \n", __LINE__, streams_.size());

		StreamMap::iterator iter = streams_.begin();
		streams_.erase(iter);

        DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) StreamManager::Create: connections_.size()=%u, after erase. \n", __LINE__, streams_.size());
    }

	unsigned int get_sid = get_saving_client_sid();
	if (get_sid + 1 == 0) get_sid = 1;
	else get_sid++;
	set_saving_client_sid(get_sid);
	s->set_stream_id(get_sid);
	streams_[get_sid] = s;

	DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, ">>> (%u) StreamManager::Create: out, sid=%d. \n", __LINE__, get_sid);

	return get_sid;
}

void StreamManager::Destroy(std::shared_ptr<IBaseStream> s)
{
	StreamMap::iterator iter;
	std::lock_guard<std::mutex> lock(stream_guard_);

	iter = streams_.begin();
	while (iter != streams_.end()) {
		if (iter->second == s) {
			streams_.erase(iter);
			break;
		}
		iter++;
	}
}

void StreamManager::DestroyAll()
{
	DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, ">>> (%u) StreamManager::DestroyAll: in, size=%d. \n", __LINE__, streams_.size());

	StreamMap::iterator iter;
	std::lock_guard<std::mutex> lock(stream_guard_);

	iter = streams_.begin();
	while (iter != streams_.end()) {
		iter->second->set_notify_remove(true);
		iter++;
	}

	streams_.clear();

	DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "<<< (%u) StreamManager::DestroyAll: out. \n", __LINE__);
}
