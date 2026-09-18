#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#pragma warning(push)
#pragma warning(disable : 4245)
#include <SimConnect.h>
#pragma warning(pop)

#include "../Commands/GSCmdReq.h"
#include "GSDefinitions.h"
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <stop_token>
#include <utility>

namespace NS_GSLiveAirportMSFS
{

class GSSimConnect
{
public:

    GSSimConnect() = default;
    virtual ~GSSimConnect() { Disconnect(); }
    GSSimConnect(const GSSimConnect &) = delete;
    GSSimConnect &operator=(const GSSimConnect &) = delete;

    virtual void OnConnect() = 0;
    virtual void OnDisconnect() = 0;
    virtual void OnSimStart() = 0;
    virtual void OnSimStop() = 0;

    void InvokeAddDatum(SIMCONNECT_DATA_DEFINITION_ID DefineID, const char *DatumName, const char *UnitsName, SIMCONNECT_DATATYPE DatumType);
    void InvokeMapClientEvent(SIMCONNECT_CLIENT_EVENT_ID EventID, const char * EventName);

    template <size_t SZ>
    void InvokeAddDatums(const std::array<GSDefinitions::DatumSpec, SZ>& arr, SIMCONNECT_DATA_DEFINITION_ID defID);

    template <typename Method, typename... Args>
    GSDefinitions::SendResult Invoke(Method&& method, Args&&... args) { return CaptureResult(method(GetHandle(), std::forward<Args>(args)...), 0U); }

    template <typename Method, typename... Args>
    GSDefinitions::SendResult InvokeRequest(DWORD nextRequestID, Method&& method, Args&&... args) { return CaptureResult(method(GetHandle(), std::forward<Args>(args)...), nextRequestID); }

    GSDefinitions::SendResult CaptureResult(HRESULT result, DWORD requestID) const;

    static void ReadMsgData(void* dest, size_t dstSize, const SIMCONNECT_RECV_SIMOBJECT_DATA& entry);

    void PostCommand(CmdPtr& cmd) { m_commands.Push(cmd); }
    void PostReqCommand(GSRequest* req);
    DWORD NextRequestID() { return m_nextRequestId++; }

protected:
    virtual void OnCommand(GSCommand& cmd) = 0;

    void RunDispatch(std::stop_token stopToken);
    void RunCommands();
    void Disconnect();  
    HANDLE GetHandle() const { return m_handle; }
    bool IsLastLoopMessage() const { return m_lastLoopMsg; }
    bool IsSimStarted() const { return m_simStarted; }
    void Connect();
    
private:
    
    void OnSimConnectMessage(SIMCONNECT_RECV *message, DWORD messageSize);
    void HandleCmdReqProcess(GSCmdReq& cmd);
    
    static void CALLBACK DispatchThunk(SIMCONNECT_RECV *message, DWORD messageSize, void *context);
    static std::size_t HashReqID(const GSDefinitions::SendResultIDs& key) noexcept { return std::hash<DWORD>{}(key.requestID); }
    static bool EqualReqID(const GSDefinitions::SendResultIDs& lhs, const GSDefinitions::SendResultIDs& rhs) noexcept { return lhs.requestID == rhs.requestID; }
    static std::size_t HashSendID(const GSDefinitions::SendResultIDs& key) noexcept { return std::hash<DWORD>{}(key.sendID); }
    static bool EqualSendID(const GSDefinitions::SendResultIDs& lhs, const GSDefinitions::SendResultIDs& rhs) noexcept { return lhs.sendID == rhs.sendID; }

    HANDLE m_handle = nullptr;
    DWORD m_nextRequestId = 10'000;
    bool m_disconnectRequested = false;
    bool m_lastDispatch = false;
    bool m_lastLoopMsg = false;
    bool m_simStarted = false;
    bool m_reportedWaiting = false;
    GSCmdQueue m_commands;

    using GSReqRef = std::reference_wrapper<GSRequest>;
    std::unordered_map<GSDefinitions::SendResultIDs, GSReqRef, decltype(&HashReqID), decltype(&EqualReqID)> m_reqIDToPtr{ 1, &HashReqID, &EqualReqID };
    std::unordered_map<GSDefinitions::SendResultIDs, GSCmdReq::ReqPtr, decltype(&HashSendID), decltype(&EqualSendID)> m_sendIDToPtr{ 1, &HashSendID, &EqualSendID };
};

template <size_t SZ>
void GSSimConnect::InvokeAddDatums(const std::array<GSDefinitions::DatumSpec, SZ>& arr, SIMCONNECT_DATA_DEFINITION_ID defID)
{
    for (const GSDefinitions::DatumSpec &datum : arr) {
        InvokeAddDatum(defID, datum.name, datum.units, datum.type);
    }        
}

} // namespace NS_GSLiveAirportMSFS
