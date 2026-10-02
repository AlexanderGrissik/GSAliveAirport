// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

namespace NS_GSAliveAirport
{

class GSLogStream
{
public:
    enum DebugLogger : std::size_t {
        // Add DBG_LOG_* entries here and matching names in s_debugLoggerNames.
        DBG_LOG_AIRCRAFT_TRACKER,
        DBG_LOG_BUGGAGE_LOADER,
        DBG_LOG_COUNT
    };

    static GSLogStream Log(std::string_view seed = "") { return GSLogStream(Level::Normal, seed); }
    static GSLogStream LogError(std::string_view seed = "") { return GSLogStream(Level::Error, seed); }
    static GSLogStream LogDebug(DebugLogger logger, std::string_view seed = "");
    static bool IsDebugEnabled(DebugLogger logger);
    static bool SetDebugLoggers(std::string_view names);
    static std::string GetDebugStatus();
    static void Print(std::string_view message);
    static void SetLoggingEnabled(bool enabled);
    [[nodiscard]] static bool LoggingEnabled() { return s_loggingEnabled.load(std::memory_order_relaxed); }
    static std::string Lower(std::string value);

    ~GSLogStream();

    GSLogStream(const GSLogStream &) = delete;
    GSLogStream &operator=(const GSLogStream &) = delete;
    GSLogStream(GSLogStream &&) = delete;
    GSLogStream &operator=(GSLogStream &&) = delete;

    template <typename T>
    GSLogStream &operator<<(const T &value)
    {
        if (m_enabled) m_oss << value;
        return *this;
    }

    GSLogStream &operator<<(std::ostream &(*manipulator)(std::ostream &))
    {
        if (m_enabled) m_oss << manipulator;
        return *this;
    }

    template <std::size_t N>
    static std::string CharArrayToString(const std::array<char, N> &arr)
    {
        std::size_t len = 0;
        while (len < arr.size() && arr[len] != 0)
            ++len;
        while (len > 0 && arr[len - 1] == ' ')
            --len;
        return std::string(arr.data(), len);
    }

private:
    enum class Level { Normal, Error, Debug };

    GSLogStream(Level level, std::string_view seed, DebugLogger logger = DBG_LOG_COUNT);
    bool IsEnabled() const { return m_enabled && (m_level != Level::Normal || LoggingEnabled()); }
    static std::string_view Trim(std::string_view text);

    static constexpr std::array<std::string_view, DBG_LOG_COUNT> s_debugLoggerNames{"GSAircraftTrackerThread","GSBuggageLoader"};
    static_assert(DBG_LOG_COUNT <= 64);
    inline static std::atomic<std::uint64_t> s_debugMask{0};
    inline static std::atomic_bool s_loggingEnabled{true};
    inline static std::mutex s_outputMutex;

    Level m_level;
    bool m_enabled;
    std::ostringstream m_oss;
};

} // namespace NS_GSAliveAirport
