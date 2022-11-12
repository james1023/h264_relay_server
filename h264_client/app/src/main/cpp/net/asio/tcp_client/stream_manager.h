#ifndef __STREAM_MANAGER_H__
#define __STREAM_MANAGER_H__

#include <map>
#include <mutex>

#include "net/asio/event/event_proc.h"

class IBaseStream;

class StreamManager
{
public:
    typedef std::map<unsigned int, std::shared_ptr<IBaseStream> > StreamMap;
    
	explicit StreamManager();
	virtual ~StreamManager();

	int Create(std::shared_ptr<IBaseStream> s);

	void Destroy(std::shared_ptr<IBaseStream> s);

	void DestroyAll();

	net::AsioEventService &aev_service() {
		return aev_service_;
	}
    
    StreamMap &streams() {
        return streams_;
    }
    
    std::mutex &stream_guard() {
        return stream_guard_;
    }

	std::shared_ptr<IBaseStream> Get(unsigned int uid) {
		std::shared_ptr<IBaseStream> find_item;
		StreamMap::iterator iter;
        
        std::lock_guard<std::mutex> lock(stream_guard());
		if ((iter = streams_.find(uid)) != streams_.end()) {
			find_item = iter->second;
		}

		return find_item;
	}
    
    int Size() {
        std::lock_guard<std::mutex> lock(stream_guard());
        return streams_.size();
    }

	unsigned int get_saving_client_sid() {
		return saving_client_sid_;
	}
	void set_saving_client_sid(const unsigned int in) {
		saving_client_sid_ = in;
	}

private:
	
	net::AsioEventService aev_service_;

	std::mutex stream_guard_;
	StreamMap streams_;
	int saving_client_sid_;
};

#endif // __STREAM_MANAGER_H__
