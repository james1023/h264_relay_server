#pragma once

#include <string>

namespace net {
namespace tcp {
namespace server {
namespace mime_types {

/// Convert a file extension into a MIME type.
std::string extension_to_type(const std::string& extension);

} // namespace mime_types
} // namespace server
} // namespace tcp
} // namespace net
