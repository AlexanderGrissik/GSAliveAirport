#pragma once

#include "GSReqBase.h"

namespace parking_services
{
// Completion object for SDK commands which have no success dispatch message.
class GSReqCommand final : public GSReqBase
{
  public:
    GSReqCommand();
    void OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override;
};
} // namespace parking_services
