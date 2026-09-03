#pragma once

#include "ISimConnectHandler.h"
#include "ISimConnectRequest.h"
#include "ISimConnectStatus.h"
#include "LogSink.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace parking_services
{
struct SendResult
{
    HRESULT result{E_FAIL};
    std::optional<DWORD> sendId;

    [[nodiscard]] bool Succeeded() const { return SUCCEEDED(result); }
};

class SimConnectThread final : public ISimConnectHandler
{
  public:
    explicit SimConnectThread(LogSink log);
    ~SimConnectThread();

    SimConnectThread(const SimConnectThread &) = delete;
    SimConnectThread &operator=(const SimConnectThread &) = delete;

    void Start();
    void Stop() override;

    void EnumerateObjects(SIMCONNECT_SIMOBJECT_TYPE type,
                          std::shared_ptr<ISimConnectRequest> request) override;
    void RequestObjectData(SIMCONNECT_DATA_DEFINITION_ID definition, DWORD objectId,
                           SIMCONNECT_PERIOD period, DWORD interval,
                           std::shared_ptr<ISimConnectRequest> request) override;
    void RequestObjectDataByType(SIMCONNECT_DATA_DEFINITION_ID definition,
                                 DWORD radiusMeters, SIMCONNECT_SIMOBJECT_TYPE type,
                                 std::shared_ptr<ISimConnectRequest> request) override;
    void CreateObject(std::string title, SIMCONNECT_DATA_INITPOSITION position,
                      std::shared_ptr<ISimConnectRequest> request) override;
    void RemoveObject(DWORD objectId,
                      std::shared_ptr<ISimConnectRequest> request) override;
    void SetObjectData(SIMCONNECT_DATA_DEFINITION_ID definition, DWORD objectId,
                       DWORD arrayCount, DWORD elementSize, const void *data,
                       std::shared_ptr<ISimConnectRequest> request) override;
    void AddToDataDefinition(SIMCONNECT_DATA_DEFINITION_ID definition,
                             std::string datumName, std::string units,
                             SIMCONNECT_DATATYPE type,
                             std::shared_ptr<ISimConnectRequest> request) override;
    void TransmitEvent(DWORD objectId, SIMCONNECT_CLIENT_EVENT_ID eventId, DWORD data,
                       std::shared_ptr<ISimConnectRequest> request) override;
    void TransmitEventEx1(DWORD objectId, SIMCONNECT_CLIENT_EVENT_ID eventId,
                          DWORD data0, DWORD data1,
                          std::shared_ptr<ISimConnectRequest> request) override;

    void RegisterStatusObserver(ISimConnectStatus *observer);
    void OnSimConnectMessage(SIMCONNECT_RECV *message, DWORD messageSize);

  private:
    static void SimConnectLoop(std::stop_token stopToken, SimConnectThread *self);
    static void CALLBACK DispatchThunk(SIMCONNECT_RECV *message, DWORD messageSize,
                                       void *context);

    void RunLoop(std::stop_token stopToken);
    void Post(std::function<void()> command);
    void ProcessCommands();
    void SweepCompletedRequests();

    bool Connect();
    void Disconnect();
    bool DefineDataAndEvents();

    void BeginEnumerateObjects(SIMCONNECT_SIMOBJECT_TYPE type,
                               std::shared_ptr<ISimConnectRequest> request);
    void BeginRequestObjectData(SIMCONNECT_DATA_DEFINITION_ID definition, DWORD objectId,
                                SIMCONNECT_PERIOD period, DWORD interval,
                                std::shared_ptr<ISimConnectRequest> request);
    void BeginRequestObjectDataByType(SIMCONNECT_DATA_DEFINITION_ID definition,
                                      DWORD radiusMeters, SIMCONNECT_SIMOBJECT_TYPE type,
                                      std::shared_ptr<ISimConnectRequest> request);
    void BeginCreateObject(std::string title, SIMCONNECT_DATA_INITPOSITION position,
                           std::shared_ptr<ISimConnectRequest> request);
    void BeginRemoveObject(DWORD objectId,
                           std::shared_ptr<ISimConnectRequest> request);
    void BeginSetObjectData(SIMCONNECT_DATA_DEFINITION_ID definition, DWORD objectId,
                            DWORD arrayCount, DWORD elementSize,
                            const std::vector<std::uint8_t> &payload,
                            std::shared_ptr<ISimConnectRequest> request);
    void BeginAddToDataDefinition(SIMCONNECT_DATA_DEFINITION_ID definition,
                                  std::string datumName, std::string units,
                                  SIMCONNECT_DATATYPE type,
                                  std::shared_ptr<ISimConnectRequest> request);
    void BeginTransmitEvent(DWORD objectId, SIMCONNECT_CLIENT_EVENT_ID eventId, DWORD data,
                            std::shared_ptr<ISimConnectRequest> request);
    void BeginTransmitEventEx1(DWORD objectId, SIMCONNECT_CLIENT_EVENT_ID eventId,
                               DWORD data0, DWORD data1,
                               std::shared_ptr<ISimConnectRequest> request);

    bool TrackResponseRequest(const SendResult &send, DWORD requestId,
                              const std::shared_ptr<ISimConnectRequest> &request);
    void CompleteCommand(const SendResult &send, std::optional<DWORD> requestId,
                         const std::shared_ptr<ISimConnectRequest> &request);
    bool RouteRequestMessage(DWORD requestId, SIMCONNECT_RECV *message, DWORD messageSize);
    bool RouteRequestFailure(const SIMCONNECT_RECV_EXCEPTION &exception);
    void RemoveRequest(const ISimConnectRequest *request);
    void FailPendingRequests();

    void HandleException(const SIMCONNECT_RECV_EXCEPTION &exception);
    void NotifyObjectRemoved(DWORD objectId);
    void NotifyConnection(bool connected);
    void NotifySimulation(bool started);

    [[nodiscard]] SendResult Capture(HRESULT result) const;
    [[nodiscard]] DWORD NextRequestId();
    bool AddDatum(SIMCONNECT_DATA_DEFINITION_ID definition, const char *name,
                  const char *units, SIMCONNECT_DATATYPE type);
    bool SubscribeSystemEvent(SIMCONNECT_CLIENT_EVENT_ID eventId, const char *name);
    bool MapClientEvent(SIMCONNECT_CLIENT_EVENT_ID eventId, const char *name);

    LogSink m_log;
    HANDLE m_handle = nullptr;
    DWORD m_nextRequestId = 10'000;
    std::vector<ISimConnectStatus *> m_statusObservers;
    // Both maps reference the same request object. Normal response packets carry
    // request IDs; exception packets carry send IDs.
    std::unordered_map<DWORD, std::shared_ptr<ISimConnectRequest>> m_requestsByRequestId;
    std::unordered_map<DWORD, std::shared_ptr<ISimConnectRequest>> m_requestsBySendId;
    bool m_reportedWaiting = false;
    bool m_disconnectRequested = false;

    std::mutex m_commandMutex;
    std::condition_variable_any m_wake;
    std::deque<std::function<void()>> m_commands;
    std::jthread m_thread;
};
} // namespace parking_services
