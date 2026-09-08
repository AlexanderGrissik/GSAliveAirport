#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#pragma warning(push)
#pragma warning(disable : 4245)
#include <SimConnect.h>
#pragma warning(pop)

#include "GSCommand.h"
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
    struct SendResult
    {
        HRESULT rc;
        DWORD sendID;
        DWORD requestID;
        bool isOK() { return SUCCEEDED(rc); }
    };

    GSSimConnect() = default;
    virtual ~GSSimConnect() { Disconnect(); }
    GSSimConnect(const GSSimConnect &) = delete;
    GSSimConnect &operator=(const GSSimConnect &) = delete;

    virtual void OnConnect() = 0;
    virtual void OnDisconnect() = 0;
    virtual void OnSimStart() = 0;
    virtual void OnSimStop() = 0;
    virtual void OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) = 0;
    virtual void OnException(SIMCONNECT_RECV_EXCEPTION *) = 0;

    virtual bool InvokeAddDatum(SIMCONNECT_DATA_DEFINITION_ID DefineID, const char *DatumName, const char *UnitsName, SIMCONNECT_DATATYPE DatumType);

    template <typename Method, typename... Args>
    SendResult Invoke(Method&& method, Args&&... args) { return CaptureResult(method(GetHandle(), std::forward<Args>(args)...), 0U); }

    template <typename Method, typename... Args>
    SendResult InvokeRequest(DWORD nextRequestID, Method&& method, Args&&... args) { return CaptureResult(method(GetHandle(), std::forward<Args>(args)...), nextRequestID); }

    SendResult CaptureResult(HRESULT result, DWORD requestID) const;

    static void ReadMsgData(void* dest, size_t dstSize, const SIMCONNECT_RECV_SIMOBJECT_DATA& entry);

    void PostCommandAndWait(int cmdId) { GSCmdQueue replyQueue; GSCommand cmd(cmdId, replyQueue); m_commands.Push(cmd); cmd.WaitPop(); }

protected:
    void RunDispatch(std::stop_token stopToken);
    void RunCommands();
    void Disconnect();

    virtual bool OnCommand(GSCommand& cmd) = 0;

    void PostCommand(GSCommand& cmd) { m_commands.Push(cmd); }

    HANDLE GetHandle() const { return m_handle; }
    DWORD NextRequestID() { return m_nextRequestId++; }

private:
    void Connect(std::stop_token stopToken);
    void OnSimConnectMessage(SIMCONNECT_RECV *message, DWORD messageSize);
    static void CALLBACK DispatchThunk(SIMCONNECT_RECV *message, DWORD messageSize, void *context);

    HANDLE m_handle = nullptr;
    DWORD m_nextRequestId = 10'000;
    bool m_disconnectRequested = false;
    bool m_lastDispatch = false;
    GSCmdQueue m_commands;
};

} // namespace NS_GSLiveAirportMSFS
