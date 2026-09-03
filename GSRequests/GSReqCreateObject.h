#pragma once

#include "GSReqBase.h"

namespace parking_services
{
class GSReqCreateObject final : public GSReqBase
{
  public:
    GSReqCreateObject();
    void OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override;
    [[nodiscard]] DWORD ObjectId() const;

  private:
    DWORD m_objectId{};
};
} // namespace parking_services
