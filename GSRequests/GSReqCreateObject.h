#pragma once

#include "GSReqBase.h"

namespace parking_services
{
class ISimConnectHandler;

class GSReqCreateObject final : public GSReqBase
{
  public:
    explicit GSReqCreateObject(ISimConnectHandler *handler = nullptr);
    void OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override;
    [[nodiscard]] DWORD ObjectId() const;

    // If a result is no longer wanted, any object assigned by MSFS is removed.
    void Abandon();

  private:
    void RemoveAbandonedObject(DWORD objectId);

    ISimConnectHandler *m_handler{};
    DWORD m_objectId{};
    bool m_abandoned{};
    bool m_removalIssued{};
};
} // namespace parking_services
