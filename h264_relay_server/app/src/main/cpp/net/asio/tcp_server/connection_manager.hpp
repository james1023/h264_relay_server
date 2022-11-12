#pragma once

#include <set>
#include <mutex>

#include "connection.hpp"

namespace net {
namespace tcp {
namespace server {

/// Manages open connections so that they may be cleanly stopped when the server
/// needs to shut down.
class connection_manager
{
public:
  /// Add the specified connection to the manager and start it.
  void start(connection_ptr c);

  /// Stop the specified connection.
  void stop(connection_ptr c);

  /// Stop all connections.
  void stop_all();

  void PushFrame(FrameFactoryStorage *frame);

private:
	/// The managed connections.
	std::set<connection_ptr> connections_;

    std::mutex push_frame_lock_;
};

} // namespace server
} // namespace tcp
} // namespace net
