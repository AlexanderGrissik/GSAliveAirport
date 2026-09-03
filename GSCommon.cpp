#include "GSCommon.h"

#include <ctime>
#include <iostream>
#include <mutex>

namespace parking_services
{
void GSLog(std::string_view message)
{
    static std::mutex logMutex;
    std::scoped_lock lock(logMutex);

    const auto now = std::time(nullptr);
    std::tm timestamp{};
    localtime_s(&timestamp, &now);

    char prefix[16];
    std::strftime(prefix, sizeof(prefix), "%d/%m/%y %H:%M", &timestamp);

    std::cout << prefix << ' ' << message << '\n';
}
} // namespace parking_services
