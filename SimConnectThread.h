#pragma once

#include "SimConnectHandler.h"
#include <condition_variable>
#include <mutex>
#include <stop_token>

namespace parking_services
{

class SimConnectThread : public SimConnectHandler
{
public:
    
    bool InvokeAddDatum(SIMCONNECT_DATA_DEFINITION_ID DefineID, const char * DatumName, const char * UnitsName, SIMCONNECT_DATATYPE DatumType) override;

protected:
    SimConnectThread() = default;
    virtual ~SimConnectThread() { Disconnect(); }

    SimConnectThread(const SimConnectThread &) = delete;
    SimConnectThread &operator=(const SimConnectThread &) = delete;

    void Disconnect();
    void RunDispatch(std::stop_token stopToken);
    void Notify() { m_eventCond.notify_all(); }

    template <typename Pred>
    void Wait(std::stop_token stopToken, Pred pred) {
       std::unique_lock lock(m_eventMutex);
       m_eventCond.wait_for(lock, stopToken, 100ms, pred);
    }

  private:

    bool Connect();
    void OnSimConnectMessage(SIMCONNECT_RECV *message, DWORD messageSize);

    DWORD NextRequestId() { return m_nextRequestId++; }

    static void CALLBACK DispatchThunk(SIMCONNECT_RECV *message, DWORD messageSize, void *context);
  
    HANDLE m_handle = nullptr;
    DWORD m_nextRequestId = 10'000;
    bool m_disconnectRequested = false;

    std::mutex m_eventMutex;
    std::condition_variable_any m_eventCond;
};
} // namespace parking_services
