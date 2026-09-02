#pragma once

#include "GSObject.h"

namespace parking_services
{
// An FSDT luggage loader (GroundServiceSpecialType::LuggageLoaderFSDT). It owns
// the cargo-door attachment specifics: placement on a right cargo door, opening
// that door, and the two-phase baggage-loader ramp alignment that positions the
// loader against the open door before it is finalized (animated + attached).
class GSLuggageLoaderFSDT final : public GSObject
{
  public:
    GSLuggageLoaderFSDT(std::uint64_t token, AircraftSnapshot aircraft,
                        GroundServiceObject object, GroundServiceLocation location);

    bool PreparePlacement(GSObjectServices &services) override;
    void Maintain(GSObjectServices &services,
                  std::chrono::steady_clock::time_point now) override;
    void OnGeometry(GSObjectServices &services, const BaggageLoaderGeometry &geometry) override;
    void OnRemoved(GSObjectServices &services) override;

  protected:
    void Activate(GSObjectServices &services, AircraftId objectId) override;

  private:
    enum class Stage { MeasureInitialGeometry, WaitForRampTarget };
    double m_rampAngleDegrees{};
    Stage m_stage{Stage::MeasureInitialGeometry};
    bool m_geometryRequested{};
    std::chrono::steady_clock::time_point m_geometryRequestDue{};
};
} // namespace parking_services