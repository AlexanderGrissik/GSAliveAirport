#pragma once

#include "GSReqBase.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace parking_services
{
using GroundObjectId = std::uint32_t;

struct GroundSnapshot
{
    GroundObjectId objectId{};
    std::string title;
    double latitude{};
    double longitude{};
    double groundSpeedKnots{};
};

struct GSGroundScanResult
{
    bool succeeded{};
    std::vector<GroundSnapshot> objects;
};

class GSReqGroundScan final : public GSReqBase
{
  public:
    GSReqGroundScan();
    void OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override;
    [[nodiscard]] GSGroundScanResult TakeResult();

  private:
    std::map<DWORD, GroundSnapshot> m_batch;
    std::vector<GroundSnapshot> m_objects;
};
} // namespace parking_services
