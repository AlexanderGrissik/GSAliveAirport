#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

namespace NS_GSLiveAirportMSFS
{

class GSLogStream
{
public:
    static GSLogStream Log(std::string_view seed = "") { return GSLogStream(false, std::string(seed)); }
    static GSLogStream LogError(std::string_view seed = "") { return GSLogStream(true, std::string(seed)); }
    static void Print(std::string_view message);
    static void SetLoggingEnabled(bool enabled);
    [[nodiscard]] static bool LoggingEnabled() { return s_loggingEnabled.load(std::memory_order_relaxed); }
    static std::string Lower(std::string value);

    GSLogStream(bool errorLevel, std::string seed);
    ~GSLogStream();

    GSLogStream(const GSLogStream &) = delete;
    GSLogStream &operator=(const GSLogStream &) = delete;
    GSLogStream(GSLogStream &&) = delete;
    GSLogStream &operator=(GSLogStream &&) = delete;

    template <typename T>
    GSLogStream &operator<<(const T &value)
    {
        m_oss << value;
        return *this;
    }

    GSLogStream &operator<<(std::ostream &(*manipulator)(std::ostream &))
    {
        m_oss << manipulator;
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
    inline static std::atomic_bool s_loggingEnabled{true};
    inline static std::mutex s_outputMutex;

    bool m_errorLevel;
    std::ostringstream m_oss;
};

} // namespace NS_GSLiveAirportMSFS