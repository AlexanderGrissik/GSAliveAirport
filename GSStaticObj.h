#pragma once

#include "GSObject.h"

namespace parking_services
{
// The ordinary ground-service object: placed once and otherwise driven by the
// common GSObject lifecycle. Its simulator position is frozen after creation.
class GSStaticObj : public GSObject
{
  public:
    GSStaticObj(AircraftSnapshot aircraft, GroundServiceObject object,
                GroundServiceLocation location);

    bool PreparePlacement(GSObjectServices &services) override;
    void ConfigureSimConnect(GSObjectServices &services) override;

  protected:
    void Activate(GSObjectServices &services, AircraftId objectId) override;

  private:
    void SetPosition(GSObjectServices &services);

    SIMCONNECT_DATA_DEFINITION_ID m_positionDefinition{};
    SIMCONNECT_CLIENT_EVENT_ID m_freezeLatitudeLongitudeEvent{};
    SIMCONNECT_CLIENT_EVENT_ID m_freezeAltitudeEvent{};
    SIMCONNECT_CLIENT_EVENT_ID m_freezeAttitudeEvent{};
};
} // namespace parking_services
