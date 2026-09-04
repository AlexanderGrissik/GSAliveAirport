#pragma once

#include "Aircraft.h"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace parking_services
{
class AnimationThread;

struct AnimationCarrier
{
    std::string name;
    double firstFrame{};
    double lastFrame{};
};

struct AnimationConfiguration
{
    double framesPerSecond{};
    bool reversible{};
    bool reversed{};
    std::vector<AnimationCarrier> carriers;
};

// World-space point in an animated object's movement path. A single point
// describes a static object; two or more points describe a path traversed
// forwards and backwards.
struct AnimationCoordinate
{
    double latitude{};
    double longitude{};
    double altitudeFeet{};
    double headingDegrees{};
};

// Complete, generic animation state prepared by GroundServicesThread. It knows
// only the target ObjectID, animation carriers, and a world-space movement path.
class AnimatedObject final
{
  public:
    AnimatedObject(AircraftId objectId, AnimationConfiguration animation,
                   std::vector<AnimationCoordinate> movementCoordinates,
                   double movementSpeedMetersPerSecond,
                   bool positionedVelocityAnimation = false);

    [[nodiscard]] AircraftId ObjectId() const { return m_objectId; }
    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] std::optional<AnimationCoordinate> ProximityTarget() const;
    bool SetDistanceRankedActive(bool active, AnimationThread &thread);
    void Start(AnimationThread &thread);
    void Animate(AnimationThread &thread);
    void Cancel(AnimationThread &thread) const;
    [[nodiscard]] bool IsMoving() const;
    [[nodiscard]] bool IsActive() const { return m_active; }

  private:
    [[nodiscard]] double CarrierValue(const AnimationCarrier &carrier,
                                      double elapsedSeconds) const;
    void AdvanceMovement(double movementSeconds);
    void QueueIdle(AnimationThread &thread) const;

    AircraftId m_objectId{};
    AnimationConfiguration m_animation;
    std::vector<AnimationCoordinate> m_movementCoordinates;
    std::vector<double> m_segmentLengths;
    double m_oneWayLengthMeters{};
    double m_routeDistanceMeters{};
    double m_movementSpeedMetersPerSecond{};
    bool m_positionedVelocityAnimation{};
    AnimationCoordinate m_currentCoordinate;
    bool m_active{};
    std::chrono::steady_clock::time_point m_animationStarted{};
    std::chrono::steady_clock::time_point m_movementUpdated{};
};
} // namespace parking_services
