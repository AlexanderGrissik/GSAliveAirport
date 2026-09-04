#pragma once

#include "GSReqBase.h"

namespace parking_services
{
struct GSObjectPoseResult
{
    bool succeeded{};
    double latitude{};
    double longitude{};
    double altitudeFeet{};
    double headingDegrees{};
};

class GSReqObjectPose final : public GSReqBase
{
  public:
    GSReqObjectPose();
    void OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override;
    [[nodiscard]] GSObjectPoseResult Result() const;

  private:
    GSObjectPoseResult m_result;
};
} // namespace parking_services
