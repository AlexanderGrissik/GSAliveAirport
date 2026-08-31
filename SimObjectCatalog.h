#pragma once

#include "SimConnectSession.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>

namespace parking_services
{
class SimObjectCatalog final
{
  public:
    explicit SimObjectCatalog(LogSink log);

    void Request(SimConnectSession &session);
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
    bool m_inProgress = false;
};
} // namespace parking_services
