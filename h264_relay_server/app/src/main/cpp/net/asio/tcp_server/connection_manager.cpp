#include <algorithm>

#include "base/debug-msg-c.h"
#include "connection_manager.hpp"

namespace net {
namespace tcp {
namespace server {

#define MAX_CONNECTIONS				25

void connection_manager::start(connection_ptr c)
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) connection_manager::start: in. \n", __LINE__);

    do {
        {
			std::lock_guard<std::mutex> lock(push_frame_lock_);
            if (connections_.size() >= MAX_CONNECTIONS) {
                DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG,
                            "(%u) connection_manager::start: connections_.size()=%u, no add. \n",
                            __LINE__, connections_.size());

                /*
                std::set<connection_ptr>::iterator iter = connections_.begin();
                (*iter)->stop();
                connections_.erase(iter);
                */

                c->stop();

                DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG,
                            "(%u) connection_manager::start: connections_.size()=%u, after.... \n",
                            __LINE__, connections_.size());

                break;
            }
            connections_.insert(c);
        }

        c->start();
    } while(0);

    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) connection_manager::start: out. \n", __LINE__);
}

void connection_manager::stop(connection_ptr c)
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) connection_manager::stop: in. \n", __LINE__);

    {
		std::lock_guard<std::mutex> lock(push_frame_lock_);
        connections_.erase(c);
    }
    c->stop();

    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) connection_manager::stop: out. \n", __LINE__);
}

void connection_manager::stop_all()
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) connection_manager::stop_all: in. \n", __LINE__);

    {
		std::lock_guard<std::mutex> lock(push_frame_lock_);
        std::for_each(connections_.begin(), connections_.end(),
                      std::bind(&connection::stop, std::placeholders::_1));

        connections_.clear();
    }

    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) connection_manager::stop_all: out. \n", __LINE__);
}

void connection_manager::PushFrame(FrameFactoryStorage *frame) 
{
    //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) connection_manager::PushFrame: in. \n", __LINE__);

    {
		std::lock_guard<std::mutex> lock(push_frame_lock_);
        for (std::set<connection_ptr>::iterator iter = connections_.begin();
             iter != connections_.end(); iter++) {
            (*iter)->PushFrame(frame);
        }
    }

    //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) connection_manager::PushFrame: out. \n", __LINE__);
}

} // namespace server
} // namespace tcp
} // namespace net
