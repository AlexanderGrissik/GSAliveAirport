#pragma once

#include "ISimConnectHandler.h"
#include "ISimConnectRequest.h"
#include "ISimConnectStatus.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
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
    SimConnectThread() = default;
    ~SimConnectThread();

    SimConnectThread(const SimConnectThread &) = delete;
    SimConnectThread &operator=(const SimConnectThread &) = delete;

    void Start();
    void Stop() override;

    void EnumerateObjects(SIMCONNECT_SIMOBJECT_TYPE type,
                          ISimConnectRequest &request) override;
    void RequestObjectData(SIMCONNECT_DATA_DEFINITION_ID definition, DWORD objectId,
                           SIMCONNECT_PERIOD period, DWORD interval,
                           ISimConnectRequest &request) override;
    void RequestObjectDataByType(SIMCONNECT_DATA_DEFINITION_ID definition,
                                 DWORD radiusMeters, SIMCONNECT_SIMOBJECT_TYPE type,
                                 ISimConnectRequest &request) override;
    void CreateObject(std::string title, SIMCONNECT_DATA_INITPOSITION position,
                      ISimConnectRequest &request) override;
    void RemoveObject(DWORD objectId,
                      ISimConnectRequest &request) override;
    void SetObjectData(SIMCONNECT_DATA_DEFINITION_ID definition, DWORD objectId,
                       DWORD arrayCount, DWORD elementSize, const void *data,
                       ISimConnectRequest &request) override;
    void AddDatum(SIMCONNECT_DATA_DEFINITION_ID definition,
                  std::string datumName, std::string units,
                  SIMCONNECT_DATATYPE type,
                  ISimConnectRequest &request) override;
    void MapClientEvent(SIMCONNECT_CLIENT_EVENT_ID eventId,
                        std::string eventName,
                        ISimConnectRequest &request) override;
    void TransmitEvent(DWORD objectId, SIMCONNECT_CLIENT_EVENT_ID eventId, DWORD data,
                       ISimConnectRequest &request) override;
    void TransmitEventEx1(DWORD objectId, SIMCONNECT_CLIENT_EVENT_ID eventId,
                          DWORD data0, DWORD data1,
                          ISimConnectRequest &request) override;

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
    bool SubscribeLifecycleEvents();

    void BeginEnumerateObjects(SIMCONNECT_SIMOBJECT_TYPE type,
                               ISimConnectRequest *request);
    void BeginRequestObjectData(SIMCONNECT_DATA_DEFINITION_ID definition, DWORD objectId,
                                SIMCONNECT_PERIOD period, DWORD interval,
                                ISimConnectRequest *request);
    void BeginRequestObjectDataByType(SIMCONNECT_DATA_DEFINITION_ID definition,
                                      DWORD radiusMeters, SIMCONNECT_SIMOBJECT_TYPE type,
                                      ISimConnectRequest *request);
    void BeginCreateObject(std::string title, SIMCONNECT_DATA_INITPOSITION position,
                           ISimConnectRequest *request);
    void BeginRemoveObject(DWORD objectId,
                           ISimConnectRequest *request);
    void BeginSetObjectData(SIMCONNECT_DATA_DEFINITION_ID definition, DWORD objectId,
                            DWORD arrayCount, DWORD elementSize,
                            const std::vector<std::uint8_t> &payload,
                            ISimConnectRequest *request);
    void BeginAddDatum(SIMCONNECT_DATA_DEFINITION_ID definition,
                       std::string datumName, std::string units,
                       SIMCONNECT_DATATYPE type,
                       ISimConnectRequest *request);
    void BeginMapClientEvent(SIMCONNECT_CLIENT_EVENT_ID eventId,
                             std::string eventName,
                             ISimConnectRequest *request);
    void BeginTransmitEvent(DWORD objectId, SIMCONNECT_CLIENT_EVENT_ID eventId, DWORD data,
                            ISimConnectRequest *request);
    void BeginTransmitEventEx1(DWORD objectId, SIMCONNECT_CLIENT_EVENT_ID eventId,
                               DWORD data0, DWORD data1,
                               ISimConnectRequest *request);

    bool TrackResponseRequest(const SendResult &send, DWORD requestId,
                              ISimConnectRequest *request);
    void CompleteCommand(const SendResult &send, std::optional<DWORD> requestId,
                         ISimConnectRequest *request);
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
    bool SubscribeSystemEvent(SIMCONNECT_CLIENT_EVENT_ID eventId, const char *name);

    HANDLE m_handle = nullptr;
    DWORD m_nextRequestId = 10'000;
    std::vector<ISimConnectStatus *> m_statusObservers;
    // Both maps hold non-owning pointers to the originator-owned request. Normal
    // response packets carry request IDs; exception packets carry send IDs.
    std::unordered_map<DWORD, ISimConnectRequest *> m_requestsByRequestId;
    std::unordered_map<DWORD, ISimConnectRequest *> m_requestsBySendId;
    bool m_reportedWaiting = false;
    bool m_disconnectRequested = false;

    std::mutex m_commandMutex;
    std::condition_variable_any m_wake;
    std::deque<std::function<void()>> m_commands;
    std::jthread m_thread;
};
} // namespace parking_services
