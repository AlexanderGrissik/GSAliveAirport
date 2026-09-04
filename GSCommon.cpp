#include "GSCommon.h"

#include <atomic>
#include <ctime>
#include <iostream>
#include <mutex>

namespace parking_services
{
namespace
{
std::atomic_bool g_loggingEnabled{false};
std::mutex g_outputMutex;
}

void GSLog(std::string_view message)
{
    if (!g_loggingEnabled.load(std::memory_order_relaxed)) return;

    std::scoped_lock lock(g_outputMutex);
    if (!g_loggingEnabled.load(std::memory_order_relaxed)) return;

    const auto now = std::time(nullptr);
    std::tm timestamp{};
    localtime_s(&timestamp, &now);

    char prefix[16];
    std::strftime(prefix, sizeof(prefix), "%d/%m/%y %H:%M", &timestamp);

    std::cout << prefix << ' ' << message << '\n';
}

void GSPrint(std::string_view message)
{
    std::scoped_lock lock(g_outputMutex);
    std::cout << message << '\n';
}

void GSSetLoggingEnabled(bool enabled)
{
    std::scoped_lock lock(g_outputMutex);
    g_loggingEnabled.store(enabled, std::memory_order_relaxed);
}

bool GSLoggingEnabled()
{
    return g_loggingEnabled.load(std::memory_order_relaxed);
}
} // namespace parking_services
