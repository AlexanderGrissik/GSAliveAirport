// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "../Commands/GSCmdReq.h"
#include "../Commands/GSCmdReqIdent.h"
#include "GSDefinitions.h"
#include <array>
#include <cstddef>
#include <functional>
#include <list>
#include <stop_token>
#include <unordered_map>
#include <utility>
#include <deque>

namespace NS_GSAliveAirport
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
    void InvokeAddFacilityDatum(SIMCONNECT_DATA_DEFINITION_ID DefineID, const char* DatumName);
    void InvokeMapClientEvent(SIMCONNECT_CLIENT_EVENT_ID EventID, const char * EventName);

    template <size_t SZ>
    void InvokeAddDatums(const std::array<GSDatum, SZ>& arr, SIMCONNECT_DATA_DEFINITION_ID defID);

    template <size_t SZ>
    void InvokeAddFacilityDatums(const std::array<const char*, SZ>& arr, SIMCONNECT_DATA_DEFINITION_ID defID);

    template <typename Method, typename... Args>
    GSInvokeRC Invoke(Method&& method, Args&&... args) { return CaptureResult(method(GetHandle(), std::forward<Args>(args)...), 0U); }

    template <typename Method, typename... Args>
    GSInvokeRC InvokeRequest(DWORD nextRequestID, Method&& method, Args&&... args) { return CaptureResult(method(GetHandle(), std::forward<Args>(args)...), nextRequestID); }

    GSInvokeRC CaptureResult(HRESULT result, DWORD requestID) const;

    static void ReadMsgData(void* dest, size_t offset, size_t dstSize, const SIMCONNECT_RECV_SIMOBJECT_DATA& entry);

    void FlushPendingRequestsWithException();
    void PostCommand(GSCommand* cmd);
    void PostReqCommand(GSRequest* req);
    void PostReqCommandDeferred(GSRequest* req, GSTimePoint execTime);
    void PostReqIdentCommand(GSRequest* req, GSCmdReqIdent::Callable&& clb);
    DWORD NextRequestID() { return m_nextRequestId++; }

protected:
    virtual void OnCommand(GSCommand& cmd) = 0;
    virtual const char* GetDebugName() const = 0;

    void RunDispatch(std::stop_token stopToken);
    void RunCommands();
    void Disconnect();  
    HANDLE GetHandle() const { return m_handle; }
    bool IsLastLoopMessage() const { return m_lastLoopMsg; }
    bool IsSimStarted() const { return m_simStarted; }
    void Connect();
    void AddRawRequestHandler(DWORD reqID, GSRequest& req) { m_reqIDToPtr.emplace(GSInvokeID{0, reqID}, req); }
    
private:
    
    void OnSimConnectMessage(SIMCONNECT_RECV *message, DWORD messageSize);
    void HandleCmdReqProcess(GSCmdReq& cmd);
    void HandleCmdReqIdentProcess(GSCmdReqIdent& cmd);
    
    static void CALLBACK DispatchThunk(SIMCONNECT_RECV *message, DWORD messageSize, void *context);
    static std::size_t HashReqID(const GSInvokeID& key) noexcept { return std::hash<DWORD>{}(key.requestID); }
    static bool EqualReqID(const GSInvokeID& lhs, const GSInvokeID& rhs) noexcept { return lhs.requestID == rhs.requestID; }
    static std::size_t HashSendID(const GSInvokeID& key) noexcept { return std::hash<DWORD>{}(key.sendID); }
    static bool EqualSendID(const GSInvokeID& lhs, const GSInvokeID& rhs) noexcept { return lhs.sendID == rhs.sendID; }

    HANDLE m_handle = nullptr;
    DWORD m_nextRequestId = 10'000;
    bool m_disconnectRequested = false;
    bool m_lastDispatch = false;
    bool m_lastLoopMsg = false;
    bool m_simStarted = false;
    bool m_reportedWaiting = false;
    GSCommandUQueue m_commands;
    std::list<GSCommandU> m_deferred;

    std::unordered_map<GSInvokeID, GSRequestR, decltype(&HashReqID), decltype(&EqualReqID)> m_reqIDToPtr{ 1, &HashReqID, &EqualReqID };
    std::unordered_map<GSInvokeID, GSRequestU, decltype(&HashSendID), decltype(&EqualSendID)> m_sendIDToPtr{ 1, &HashSendID, &EqualSendID };
    std::deque<std::pair<GSInvokeID, GSCmdReqIdent::Callable>> m_sendIDToIdent;
};

template <size_t SZ>
void GSSimConnect::InvokeAddDatums(const std::array<GSDatum, SZ>& arr, SIMCONNECT_DATA_DEFINITION_ID defID)
{
    for (const GSDatum &datum : arr) {
        InvokeAddDatum(defID, datum.name, datum.units, datum.type);
    }        
}

template <size_t SZ>
void GSSimConnect::InvokeAddFacilityDatums(const std::array<const char*, SZ>& arr, SIMCONNECT_DATA_DEFINITION_ID defID)
{
    for (const auto& datum : arr) {
        InvokeAddFacilityDatum(defID, datum);
    }
}

} // namespace NS_GSAliveAirport
