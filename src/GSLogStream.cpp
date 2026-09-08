#include "GSLogStream.h"

#include <algorithm>
#include <ctime>
#include <iostream>

namespace NS_GSLiveAirportMSFS
{

GSLogStream::GSLogStream(bool errorLevel, std::string seed) : m_errorLevel(errorLevel)
{
    if (!seed.empty()) {
        m_oss << seed;
    }
}

GSLogStream::~GSLogStream()
{
    if (!m_errorLevel && !s_loggingEnabled.load(std::memory_order_relaxed)) {
        return;
    }

    std::scoped_lock lock(s_outputMutex);
    if (!m_errorLevel && !s_loggingEnabled.load(std::memory_order_relaxed)) {
        return;
    }

    const auto now = std::time(nullptr);
    std::tm timestamp{};
    localtime_s(&timestamp, &now);

    char prefix[16];
    std::strftime(prefix, sizeof(prefix), "%d/%m/%y %H:%M", &timestamp);

    std::string text = m_oss.str();
    if (!text.empty() && text.back() != '\n') {
        text.push_back('\n');
    }

    std::cout << prefix << ' ' << text;
}

void GSLogStream::Print(std::string_view message)
{
    std::scoped_lock lock(s_outputMutex);
    std::cout << message << '\n';
}

void GSLogStream::SetLoggingEnabled(bool enabled)
{
    std::scoped_lock lock(s_outputMutex);
    s_loggingEnabled.store(enabled, std::memory_order_relaxed);
}

std::string GSLogStream::Lower(std::string value)
{
    std::ranges::transform(value, value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

} // namespace NS_GSLiveAirportMSFS