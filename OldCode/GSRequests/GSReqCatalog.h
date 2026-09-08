#pragma once

#include "../ISimConnectRequest.h"
#include "../SimObjectCatalog.h"

#include <condition_variable>
#include <mutex>
#include <vector>

namespace parking_services
{
class GSReqCatalog final : public ISimConnectRequest
{
  public:
    using Entry = SimObjectCatalog::Entry;

    void OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override;
    void OnSuccess() override;
    void OnFailure() override;
    [[nodiscard]] bool IsComplete() const override;

    void Wait();
    [[nodiscard]] bool Succeeded() const;
    [[nodiscard]] const std::vector<Entry> &Entries() const { return m_entries; }

  private:
    enum class State { Pending, Succeeded, Failed };

    void Finish(State state);

    mutable std::mutex m_mutex;
    std::condition_variable m_completed;
    std::vector<Entry> m_entries;
    bool m_responseComplete{};
    bool m_responseSucceeded{};
    State m_state{State::Pending};
};
} // namespace parking_services
