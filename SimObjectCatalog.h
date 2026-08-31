#pragma once

#include "SimConnectSession.h"

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace parking_services
{
class SimObjectCatalog final
{
  public:
    using CompletionCallback = std::function<void(std::vector<std::string>)>;

    explicit SimObjectCatalog(LogSink log);

    void Request(SimConnectSession &session, CompletionCallback callback = {});
    void HandleData(SimConnectSession &session,
                    const SIMCONNECT_RECV_ENUMERATE_SIMOBJECT_AND_LIVERY_LIST &message,
                    DWORD messageSize);
    void Reset();
    [[nodiscard]] bool InProgress() const;

  private:
    using Entry = std::pair<std::string, std::string>;
    struct Bucket
    {
        std::string label;
        SIMCONNECT_SIMOBJECT_TYPE type{};
        bool excluded{};
        bool complete{};
        std::set<Entry> entries;
    };

    void FinishIfComplete();

    LogSink m_log;
    std::map<DWORD, Bucket> m_buckets;
    std::optional<DWORD> m_allRequestId;
    std::vector<CompletionCallback> m_callbacks;
    bool m_inProgress = false;
};
} // namespace parking_services
