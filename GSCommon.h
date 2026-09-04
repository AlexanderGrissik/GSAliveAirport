#pragma once

#include <string_view>

namespace parking_services
{
// Background logging is disabled by default. Calls from all worker threads are
// serialized by the implementation.
void GSLog(std::string_view message);
void GSPrint(std::string_view message);
void GSSetLoggingEnabled(bool enabled);
[[nodiscard]] bool GSLoggingEnabled();
} // namespace parking_services
