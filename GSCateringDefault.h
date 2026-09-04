#pragma once

#include "GSObject.h"

namespace parking_services
{
// Asobo catering truck (ASO_Catering_Truck_01). It parks at the aircraft's
// rear-right passenger (main exit) door and extends its serving hatch to the
// door sill. On activation it sets the container elevation to the door height
// (CATERINGTRUCK ELEVATION TARGET, meters) and deploys the bridge
// (CATERINGTRUCK OPENING TARGET = 1); it also best-effort opens the aircraft's
// rear door. On removal the hatch is stowed and the door closed.
class GSCateringDefault final : public GSObject
{
  public:
    GSCateringDefault(AircraftSnapshot aircraft, GroundServiceObject object,
                      GroundServiceLocation location);

    bool PreparePlacement(GSObjectServices &services) override;
    void ConfigureSimConnect(GSObjectServices &services) override;
    void OnRemoved(GSObjectServices &services) override;

  protected:
    void Activate(GSObjectServices &services, AircraftId objectId) override;

  private:
    [[nodiscard]] static const AircraftCargoConnectionPoint *SelectRearRightDoor(
        const std::vector<AircraftCargoConnectionPoint> &exits);
    void FreezeObject(GSObjectServices &services, AircraftId objectId);
    void SetPosition(GSObjectServices &services, AircraftId objectId);
    void SetElevation(GSObjectServices &services, AircraftId objectId,
                      double meters);
    void SetOpening(GSObjectServices &services, AircraftId objectId, bool open);
    void SetAircraftDoor(GSObjectServices &services, bool open);

    std::optional<AircraftCargoConnectionPoint> m_door;
    double m_doorSillMeters{};
    SIMCONNECT_DATA_DEFINITION_ID m_positionDefinition{};
    SIMCONNECT_DATA_DEFINITION_ID m_elevationTargetDefinition{};
    SIMCONNECT_DATA_DEFINITION_ID m_openingTargetDefinition{};
    SIMCONNECT_CLIENT_EVENT_ID m_freezeLatitudeLongitudeEvent{};
    SIMCONNECT_CLIENT_EVENT_ID m_freezeAltitudeEvent{};
    SIMCONNECT_CLIENT_EVENT_ID m_freezeAttitudeEvent{};
    SIMCONNECT_CLIENT_EVENT_ID m_openAircraftDoorsEvent{};
    SIMCONNECT_CLIENT_EVENT_ID m_closeAircraftDoorsEvent{};
};
} // namespace parking_services