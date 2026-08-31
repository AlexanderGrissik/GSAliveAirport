#pragma once

#include <functional>
#include <string>

namespace parking_services
{
using LogSink = std::function<void(std::string)>;
} // namespace parking_services
