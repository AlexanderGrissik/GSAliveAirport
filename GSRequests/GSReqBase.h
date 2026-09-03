#pragma once

#include "../ISimConnectRequest.h"

#include <chrono>
#include <mutex>
#include <optional>

namespace parking_services
{
// Shared status and timeout handling for finite SimConnect requests.
class GSReqBase : public ISimConnectRequest
{
  public:
    ~GSReqBase() override = default;

    void OnSuccess() override;
    void OnFailure() override;
    [[nodiscard]] bool IsComplete() const final;
    [[nodiscard]] bool IsFinished() const;
    [[nodiscard]] bool Succeeded() const;

  protected:
    explicit GSReqBase(
        std::optional<std::chrono::steady_clock::duration> timeout);

    enum class State { Pending, Succeeded, Failed };

    [[nodiscard]] bool PendingLocked() const;
    void CompleteLocked(bool succeeded);
    void ExpireLocked() const;

    mutable std::mutex m_mutex;
    mutable State m_state{State::Pending};

  private:
    mutable bool m_responseComplete{};
    mutable bool m_responseSucceeded{};
    std::optional<std::chrono::steady_clock::time_point> m_deadline;
};
} // namespace parking_services
