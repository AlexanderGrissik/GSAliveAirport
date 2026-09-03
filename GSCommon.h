#pragma once

#include <string_view>

namespace parking_services
{
// Process-wide logging entry point. Calls from all worker threads are
// serialized by the implementation.
void GSLog(std::string_view message);
} // namespace parking_services
