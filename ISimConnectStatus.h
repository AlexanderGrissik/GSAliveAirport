#pragma once

#include <cstdint>

namespace parking_services
{
// Implemented by a worker thread that must react when SimConnect (re)connects
// to the simulator. SimConnectThread invokes these on its own dispatch thread,
// so an implementation must only ENQUEUE work to its own thread (Post) and
// return immediately -- it must never block and must never touch the SimConnect
// session. Implementers register with SimConnectThread at startup through
// RegisterStatusObserver.
class ISimConnectStatus
{
  public:
    virtual ~ISimConnectStatus() = default;
    virtual void OnSimConnected() = 0;
    virtual void OnSimDisconnected() = 0;
    virtual void OnSimStarted() = 0;
    virtual void OnSimStopped() = 0;
    virtual void OnObjRemoved(std::uint32_t objectId) = 0;
};
} // namespace parking_services