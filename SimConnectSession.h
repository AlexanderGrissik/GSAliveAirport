#pragma once

#include "LogSink.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Windows.h>
#include <SimConnect.h>

#include <atomic>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace parking_services
{
class ISimConnectMessageSink
{
  public:
    virtual ~ISimConnectMessageSink() = default;
    virtual void OnSimConnectMessage(SIMCONNECT_RECV *message, DWORD messageSize) = 0;
};

struct SendResult
{
    HRESULT result{E_FAIL};
    std::optional<DWORD> packetId;

    [[nodiscard]] bool Succeeded() const { return SUCCEEDED(result); }
};

class SimConnectSession final
{
  public:
    explicit SimConnectSession(LogSink log);
    ~SimConnectSession();

    SimConnectSession(const SimConnectSession &) = delete;
    SimConnectSession &operator=(const SimConnectSession &) = delete;

    bool Open(ISimConnectMessageSink &sink);
    void Close();
    [[nodiscard]] bool IsConnected() const;
    HRESULT Dispatch();

    bool AddDatum(SIMCONNECT_DATA_DEFINITION_ID definition, const char *name, const char *units,
                  SIMCONNECT_DATATYPE type);
    bool SubscribeSystemEvent(SIMCONNECT_CLIENT_EVENT_ID eventId, const char *name);
    bool MapClientEvent(SIMCONNECT_CLIENT_EVENT_ID eventId, const char *name);

    SendResult RequestObjectsByType(SIMCONNECT_DATA_REQUEST_ID requestId,
                                    SIMCONNECT_DATA_DEFINITION_ID definitionId,
                                    DWORD radiusMeters, SIMCONNECT_SIMOBJECT_TYPE type);
    SendResult RequestObjectData(SIMCONNECT_DATA_REQUEST_ID requestId,
                                 SIMCONNECT_DATA_DEFINITION_ID definitionId, DWORD objectId,
                                 SIMCONNECT_PERIOD period, DWORD interval = 0);
    SendResult EnumerateObjects(SIMCONNECT_DATA_REQUEST_ID requestId,
                                SIMCONNECT_SIMOBJECT_TYPE type);
    SendResult TransmitEvent(DWORD objectId, SIMCONNECT_CLIENT_EVENT_ID eventId, DWORD data);
    SendResult SetObjectData(SIMCONNECT_DATA_DEFINITION_ID definitionId, DWORD objectId,
                             DWORD arrayCount, DWORD elementSize, const void *data);
    SendResult CreateObject(std::string_view title, const SIMCONNECT_DATA_INITPOSITION &position,
                            DWORD requestId);
    SendResult RemoveObject(DWORD objectId, DWORD requestId);

    DWORD NextRequestId();
    void TrackOperation(const SendResult &send, DWORD requestId, std::string description,
                        std::function<void()> onFailure = {});
    void CompleteRequest(DWORD requestId);
    std::optional<std::string> HandleException(DWORD sendId);

  private:
    static void CALLBACK DispatchThunk(SIMCONNECT_RECV *message, DWORD messageSize, void *context);
    SendResult Capture(HRESULT result) const;

    struct PendingOperation
    {
        DWORD requestId{};
        std::string description;
        std::function<void()> onFailure;
    };

    LogSink m_log;
    HANDLE m_handle = nullptr;
    ISimConnectMessageSink *m_sink = nullptr;
    std::atomic_bool m_connected{false};
    DWORD m_nextRequestId = 10'000;
    std::unordered_map<DWORD, PendingOperation> m_pendingOperations;
};
} // namespace parking_services
