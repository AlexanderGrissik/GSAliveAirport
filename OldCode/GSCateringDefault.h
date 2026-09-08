#pragma once

#include "GSObject.h"

#include <memory>

namespace parking_services
{
class GSReqCateringState;

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
    ~GSCateringDefault() override;

    bool PreparePlacement(GSObjectServices &services) override;
    void ConfigureSimConnect(GSObjectServices &services) override;
    void Maintain(GSObjectServices &services,
                  std::chrono::steady_clock::time_point now) override;
    void OnRemoved(GSObjectServices &services) override;

  protected:
    void Activate(GSObjectServices &services, AircraftId objectId) override;
    [[nodiscard]] bool SpecialRequestsFinished() const override;

  private:
    enum class Stage { AlignDoorContact, WaitForElevation, WaitForOpening };

    [[nodiscard]] static const AircraftCargoConnectionPoint *SelectRearRightDoor(
        const std::vector<AircraftCargoConnectionPoint> &exits);
    void RequestState(GSObjectServices &services);
    void FinishAfterTimeout(GSObjectServices &services);
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
    SIMCONNECT_DATA_DEFINITION_ID m_stateDefinition{};
    SIMCONNECT_CLIENT_EVENT_ID m_freezeLatitudeLongitudeEvent{};
    SIMCONNECT_CLIENT_EVENT_ID m_freezeAltitudeEvent{};
    SIMCONNECT_CLIENT_EVENT_ID m_freezeAttitudeEvent{};
    SIMCONNECT_CLIENT_EVENT_ID m_openAircraftDoorsEvent{};
    SIMCONNECT_CLIENT_EVENT_ID m_closeAircraftDoorsEvent{};
    std::unique_ptr<GSReqCateringState> m_stateRequest;
    Stage m_stage{Stage::AlignDoorContact};
    std::chrono::steady_clock::time_point m_stateRequestDue{};
    std::chrono::steady_clock::time_point m_activationDeadline{};
    double m_lastElevationCurrent{};
    double m_lastElevationTarget{};
    double m_lastOpeningCurrent{};
    double m_initialElevationMeters{};
    int m_elevationTargetMismatchCount{};
    bool m_haveState{};
    bool m_haveInitialElevation{};
    bool m_simConnectConfigured{};
};
} // namespace parking_services
