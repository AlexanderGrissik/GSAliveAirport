#pragma once

#include "GSReqBase.h"
#include "../GroundObject.h"

#include <map>
#include <vector>

namespace parking_services
{
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
