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
class ISimConnectRequest
{
  public:
    virtual ~ISimConnectRequest() = default;

    // Response-bearing SDK operations receive their real dispatch message.
    // Operations with no success reply receive nullptr after the SDK accepts
    // the command. IsComplete tells the SimConnect thread when the request has
    // consumed all expected messages. The terminal callback is made only after
    // the thread has removed the request from all of its tracking structures.
    virtual void OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) = 0;
    virtual void OnSuccess() = 0;
    virtual void OnFailure() = 0;
    [[nodiscard]] virtual bool IsComplete() const = 0;
};
} // namespace parking_services
