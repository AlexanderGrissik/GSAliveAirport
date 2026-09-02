#pragma once

#include "AnimationObject.h"

namespace parking_services
{
class AnimationConfiguredObject final : public AnimationObject
{
  public:
    AnimationConfiguredObject(AircraftId objectId, GroundServiceAnimation animation,
                              std::optional<AnimationRoute> route);

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] bool OwnsObject(AircraftId objectId) const override;
    [[nodiscard]] std::optional<AnimationProximityTarget>
        ProximityTarget() const override;
    bool SetDistanceRankedActive(bool active, AnimationFrame &frame) override;
    void Start(AnimationFrame &frame) override;
    void Animate(AnimationFrame &frame) override;
    void Cancel(AnimationFrame &frame) const override;
    [[nodiscard]] bool IsWalking() const override;

  private:
    [[nodiscard]] double CarrierValue(const GroundServiceAnimationCarrier &carrier,
                                      double elapsedSeconds) const;
    void QueueIdle(AnimationFrame &frame) const;

    AircraftId m_objectId{};
    GroundServiceAnimation m_animation;
    std::optional<AnimationRoute> m_route;
    double m_routeLengthMeters{};
    double m_routeDistanceMeters{};
    double m_currentLatitude{};
    double m_currentLongitude{};
    double m_currentAltitudeFeet{};
    double m_currentHeadingDegrees{};
    bool m_active{};
    std::chrono::steady_clock::time_point m_animationStarted{};
    std::chrono::steady_clock::time_point m_movementUpdated{};
};
} // namespace parking_services
