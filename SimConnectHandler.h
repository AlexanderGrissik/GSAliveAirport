#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#pragma warning(push)
#pragma warning(disable : 4245) // SimConnect SDK enum declarations use signed literals.
#include <SimConnect.h>
#pragma warning(pop)

namespace parking_services
{

// Thread-safe command surface for the SimConnect dispatch thread. Every SDK
// operation receives its own request object. The originator owns that object
// until OnSuccess or OnFailure marks it finished. SimConnectThread owns only
// request/send IDs and keeps non-owning references while routing SDK messages.
class SimConnectHandler
{
public:
    struct SendResult
    {
        HRESULT rc;
        DWORD sendID;
        DWORD requestID;

        bool isOK() { SUCCEEDED(rc); }
    };

    virtual ~SimConnectHandler() = default;
    virtual DWORD NextRequestID() = 0;
    virtual HANDLE GetHandle() = 0;
    
    virtual void OnConnect() = 0;
    virtual void OnDisconnect() = 0;
    virtual void OnSimStart() = 0;
    virtual void OnSimStop() = 0;
    virtual void OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) = 0;
    virtual void OnException(SIMCONNECT_RECV_EXCEPTION *) = 0;

    virtual bool InvokeAddDatum(SIMCONNECT_DATA_DEFINITION_ID DefineID, const char * DatumName, const char * UnitsName, SIMCONNECT_DATATYPE DatumType) = 0;

    template <typename Method, typename... Args>
    SendResult Invoke(Method&& method, Args&&... args) {
        return CaptureResult(method(GetHandle(), std::forward<Args>(args)...), 0U);
    }

    template <typename Method, typename... Args>
    SendResult InvokeRequest(DWORD nextRequestID, Method&& method, Args&&... args) {
        return CaptureResult(method(GetHandle(), std::forward<Args>(args)...), nextRequestID);
    }

    SendResult CaptureResult(HRESULT result, DWORD requestID) const
    {
        DWORD sendId = 0;
        if (m_handle && SUCCEEDED(result)) {
            if (!SUCCEEDED(SimConnect_GetLastSentPacketID(m_handle, &sendId))) {
                sendId = 0;
            }
        }
        return {result, sendId, requestID};
    }

    static void ReadMsgData(void* dest, size_t dstSize, const SIMCONNECT_RECV_SIMOBJECT_DATA& entry);
};
} // namespace parking_services
