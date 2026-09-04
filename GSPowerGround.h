#pragma once

#include "GSObject.h"

namespace parking_services
{
// Built-in / Asobo Ground Power Unit. It behaves exactly like an ordinary
// ground-service object (placed once from a configured static location and
// frozen after creation); the only addition is that on activation it deploys
// its hose (GROUNDPOWERUNIT HOSE DEPLOYED) when the aircraft exposes a Ground
// Power cable receptacle. When the aircraft has no receptacle the unit stays in
// place and fully visible, with the hose left stowed.
class GSPowerGround final : public GSObject
{
  public:
    GSPowerGround(AircraftSnapshot aircraft, GroundServiceObject object,
                  GroundServiceLocation location);

    bool PreparePlacement(GSObjectServices &services) override;
    void ConfigureSimConnect(GSObjectServices &services) override;
    bool RepositionRelative(GSObjectServices &services, double x, double y,
                            double z, double headingDegrees) override;

  protected:
    void Activate(GSObjectServices &services, AircraftId objectId) override;
    void OnParentRepositioned(GSObjectServices &services,
                              const GSObjectPos &parentPose) override;

  private:
    [[nodiscard]] bool HasAircraftGroundPower() const;
    void ApplyParentPose(GSObjectServices &services, const GSObjectPos &parentPose);
    void SetPosition(GSObjectServices &services);
    void SetHoseDeployed(GSObjectServices &services, AircraftId objectId,
                         bool deployed);

    SIMCONNECT_DATA_DEFINITION_ID m_positionDefinition{};
    SIMCONNECT_DATA_DEFINITION_ID m_hoseDeployedDefinition{};
    SIMCONNECT_CLIENT_EVENT_ID m_freezeLatitudeLongitudeEvent{};
    SIMCONNECT_CLIENT_EVENT_ID m_freezeAltitudeEvent{};
    SIMCONNECT_CLIENT_EVENT_ID m_freezeAttitudeEvent{};
};
} // namespace parking_services
