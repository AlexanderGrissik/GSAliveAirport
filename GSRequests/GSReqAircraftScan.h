#pragma once

#include "GSReqBase.h"
#include "../Aircraft.h"

#include <map>
#include <vector>

namespace parking_services
{
struct GSAircraftScanResult
{
    bool succeeded{};
    std::vector<AircraftSnapshot> aircraft;
};

class GSReqAircraftScan final : public GSReqBase
{
  public:
    GSReqAircraftScan();
    void OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override;
    [[nodiscard]] GSAircraftScanResult TakeResult();

  private:
    std::map<DWORD, AircraftSnapshot> m_batch;
    std::vector<AircraftSnapshot> m_aircraft;
};
} // namespace parking_services
