#include "GSLogStream.h"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iostream>

namespace NS_GSLiveAirportMSFS
{

GSLogStream::GSLogStream(Level level, std::string_view seed, DebugLogger logger)
    : m_level(level), m_enabled(level != Level::Debug || IsDebugEnabled(logger))
{
    if (!m_enabled) return;
    if (level == Level::Debug) m_oss << '[' << s_debugLoggerNames[logger] << "] ";
    m_oss << seed;
}

GSLogStream GSLogStream::LogDebug(DebugLogger logger, std::string_view seed)
{
    return GSLogStream(Level::Debug, seed, logger);
}

bool GSLogStream::IsDebugEnabled(DebugLogger logger)
{
    return logger < DBG_LOG_COUNT && (s_debugMask.load(std::memory_order_relaxed) & (std::uint64_t{1} << logger)) != 0;
}

bool GSLogStream::SetDebugLoggers(std::string_view names)
{
    const std::string NAMES = Lower(std::string(Trim(names)));
    std::uint64_t mask = 0;
    if (NAMES == "all") {
        for (std::size_t index = 0; index < s_debugLoggerNames.size(); ++index) {
            mask |= std::uint64_t{1} << index;
        }
    } else if (NAMES != "off") {
        std::string_view remaining = NAMES;
        while (true) {
            const auto COMMA = remaining.find(',');
            const auto NAME = Trim(remaining.substr(0, COMMA));
            if (NAME.empty()) return false;
            const auto LOGGER = std::ranges::find_if(s_debugLoggerNames, [NAME](std::string_view name) {
                return Lower(std::string(name)) == NAME;
            });
            if (LOGGER == s_debugLoggerNames.end()) return false;
            mask |= std::uint64_t{1} << (LOGGER - s_debugLoggerNames.begin());
            if (COMMA == std::string_view::npos) break;
            remaining.remove_prefix(COMMA + 1);
        }
    }
    s_debugMask.store(mask, std::memory_order_relaxed);
    return true;
}

std::string GSLogStream::GetDebugStatus()
{
    if (s_debugLoggerNames.empty()) return "Debug loggers: none registered.";
    const auto MASK = s_debugMask.load(std::memory_order_relaxed);
    std::string status = "Debug loggers: ";
    for (std::size_t index = 0; index < s_debugLoggerNames.size(); ++index) {
        if (index != 0) status += ", ";
        status += s_debugLoggerNames[index];
        status += (MASK & (std::uint64_t{1} << index)) ? "=on" : "=off";
    }
    return status;
}

std::string_view GSLogStream::Trim(std::string_view text)
{
    const auto BEGIN = text.find_first_not_of(" \t\r\n");
    if (BEGIN == std::string_view::npos) return {};
    return text.substr(BEGIN, text.find_last_not_of(" \t\r\n") - BEGIN + 1);
}

GSLogStream::~GSLogStream()
{
    if (!IsEnabled()) {
        return;
    }

    std::scoped_lock lock(s_outputMutex);
    if (!IsEnabled()) {
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