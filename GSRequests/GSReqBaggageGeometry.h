#pragma once

#include "GSReqBase.h"

namespace parking_services
{
struct GSBaggageGeometryResult
{
    bool succeeded{};
    double angleCurrentDegrees{};
    double endRampYMeters{};
    double endRampZMeters{};
    double pivotYMeters{};
    double pivotZMeters{};
};

class GSReqBaggageGeometry final : public GSReqBase
{
  public:
    GSReqBaggageGeometry();
    void OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override;
    [[nodiscard]] GSBaggageGeometryResult Result() const;

  private:
    GSBaggageGeometryResult m_result;
};
} // namespace parking_services
