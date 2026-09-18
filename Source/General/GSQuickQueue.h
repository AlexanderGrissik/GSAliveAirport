#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <chrono>
#include <thread>
#include <type_traits>
#include <optional>
#include <new>
#include <intrin.h>
#include <utility>

template <typename T, std::size_t CAP>
class GSQuickQueue
{
    static_assert(CAP >= 2, "GSQuickQueue CAP must be at least 2");
    static_assert((CAP & (CAP - 1)) == 0, "GSQuickQueue CAP must be power of two");

    using storage_type = typename std::aligned_storage<sizeof(T), alignof(T)>::type;

    class ProducerGuard final
    {
    public:
        ProducerGuard(std::atomic_flag& lock) : m_lock(lock) {
            while (m_lock.test_and_set(std::memory_order_acquire)) {
                _mm_pause();
            }
        }

        ~ProducerGuard() { m_lock.clear(std::memory_order_release); }
    private:
        std::atomic_flag& m_lock;
    };

    static constexpr size_t s_cacheLineSize = 64;
    static constexpr auto s_longWait = std::chrono::milliseconds(1);
    static constexpr size_t ItemIdex(size_t pos) { return pos & (CAP - 1); }

    T* valPos(size_t pos) { return std::launder(reinterpret_cast<T*>(&m_array[ItemIdex(pos)])); }

    alignas(s_cacheLineSize) std::array<storage_type, CAP> m_array;
    std::atomic_size_t m_push_pos;
    std::atomic_size_t m_pop_pos;
    std::atomic_flag m_producerLock{};

public:

    GSQuickQueue() = default;

    ~GSQuickQueue() {
        size_t popPos = m_pop_pos.load(std::memory_order_relaxed);
        size_t pushPos = m_push_pos.load(std::memory_order_relaxed);

        while (popPos != pushPos) {
            valPos(popPos)->~T();
            ++popPos;
        }
    }

    void Push(T& val) {
        ProducerGuard grd(m_producerLock);

        size_t pushPos = m_push_pos.load(std::memory_order_relaxed);
        while (pushPos - m_pop_pos.load(std::memory_order_acquire) >= CAP) {
            std::this_thread::sleep_for(s_longWait);
        }

        ::new (static_cast<void*>(&m_array[ItemIdex(pushPos)])) T(std::move(val));

        m_push_pos.store(pushPos + 1, std::memory_order_release);
    }

    T Pop() {
        size_t popPos = m_pop_pos.load(std::memory_order_relaxed);
        while (m_push_pos.load(std::memory_order_acquire) == popPos) {
            std::this_thread::sleep_for(s_longWait);
        }

        T* value = valPos(popPos);
        T rc(std::move(*value));
        value->~T();

        m_pop_pos.store(popPos + 1, std::memory_order_release);
        return rc;
    }

    [[nodiscard]] std::optional<T> TryPop() {
        size_t popPos = m_pop_pos.load(std::memory_order_relaxed);
        if (m_push_pos.load(std::memory_order_acquire) == popPos) {
            return std::nullopt;
        }

        T* value = valPos(popPos);
        std::optional<T> rc(std::in_place, std::move(*value));
        value->~T();

        m_pop_pos.store(popPos + 1, std::memory_order_release);
        return rc;
    }
};
