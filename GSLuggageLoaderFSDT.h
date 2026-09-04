#pragma once

#include "GSObject.h"
#include "GSRequests/GSReqBaggageGeometry.h"

#include <memory>
#include <optional>

namespace parking_services
{
// Self-contained FSDT luggage-loader behavior. GroundServicesThread sees only
// the GSObject lifecycle and never handles baggage geometry, ramp data, or door
// events directly.
class GSLuggageLoaderFSDT final : public GSObject
{
  public:
    GSLuggageLoaderFSDT(AircraftSnapshot aircraft, GroundServiceObject object,
                        GroundServiceLocation location);

    bool PreparePlacement(GSObjectServices &services) override;
    void ConfigureSimConnect(GSObjectServices &services) override;
    void Maintain(GSObjectServices &services,
                  std::chrono::steady_clock::time_point now) override;
    void OnRemoved(GSObjectServices &services) override;

  protected:
    void Activate(GSObjectServices &services, AircraftId objectId) override;
    [[nodiscard]] bool SpecialRequestsFinished() const override;

  private:
    struct CargoDoor
    {
        std::uint32_t interactivePointIndex{};
        double forwardMeters{};
        double rightMeters{};
        double heightMeters{};
        double modelRelativeHeadingDegrees{};
    };

    struct RampTargetWireData
    {
        double angleDegrees{};
    };

    enum class Stage { MeasureInitialGeometry, WaitForRampTarget };

    static constexpr double CargoDoorClearanceMeters = 0.5;

    void RequestGeometry(GSObjectServices &services);
    void HandleGeometry(GSObjectServices &services,
                        const GSBaggageGeometryResult &geometry);
    void FreezeObject(GSObjectServices &services, AircraftId objectId);
    void SetPosition(GSObjectServices &services, AircraftId objectId,
                     const GSObjectPos &pose);
    void SetRampTarget(GSObjectServices &services, AircraftId objectId,
                       double angleDegrees);
    void SetCargoDoor(GSObjectServices &services, bool open);

    std::optional<CargoDoor> m_cargoDoor;
    std::unique_ptr<GSReqBaggageGeometry> m_geometryRequest;
    SIMCONNECT_DATA_DEFINITION_ID m_rampTargetDefinition{};
    SIMCONNECT_DATA_DEFINITION_ID m_geometryDefinition{};
    SIMCONNECT_DATA_DEFINITION_ID m_positionDefinition{};
    SIMCONNECT_CLIENT_EVENT_ID m_freezeLatitudeLongitudeEvent{};
    SIMCONNECT_CLIENT_EVENT_ID m_freezeAltitudeEvent{};
    SIMCONNECT_CLIENT_EVENT_ID m_freezeAttitudeEvent{};
    SIMCONNECT_CLIENT_EVENT_ID m_openAircraftDoorsEvent{};
    SIMCONNECT_CLIENT_EVENT_ID m_closeAircraftDoorsEvent{};
    double m_rampAngleDegrees{};
    Stage m_stage{Stage::MeasureInitialGeometry};
    std::chrono::steady_clock::time_point m_geometryRequestDue{};
    bool m_simConnectConfigured{};
};
} // namespace parking_services
