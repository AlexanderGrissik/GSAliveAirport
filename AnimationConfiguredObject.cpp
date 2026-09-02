#include "AnimationConfiguredObject.h"

#include <cctype>
#include <cmath>
#include <utility>

namespace parking_services
{
namespace
{
constexpr double kWalkingSpeedMetersPerSecond = 2.5 * 0.514444;
constexpr double kDegreesPerRadian = 180.0 / 3.14159265358979323846;
constexpr std::string_view kVelocityBodyYCarrier = "VELOCITY BODY Y";

bool EqualIgnoreCase(std::string_view left, std::string_view right)
{
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (std::tolower(static_cast<unsigned char>(left[index])) !=
            std::tolower(static_cast<unsigned char>(right[index]))) {
            return false;
        }
    }
    return true;
}
} // namespace

AnimationConfiguredObject::AnimationConfiguredObject(
    AircraftId objectId, GroundServiceAnimation animation,
    std::optional<AnimationRoute> route)
    : m_objectId(objectId), m_animation(std::move(animation)), m_route(std::move(route))
{
    if (!m_route) return;
    m_currentLatitude = m_route->latitude1;
    m_currentLongitude = m_route->longitude1;
    m_currentAltitudeFeet = m_route->altitude1Feet;
    const double segment = DistanceMeters(m_route->latitude1, m_route->longitude1,
                                          m_route->latitude2, m_route->longitude2);
    m_routeLengthMeters = segment * 2.0;
    if (segment > 0.0) {
        m_currentHeadingDegrees = std::fmod(
            std::atan2(m_route->longitude2 - m_route->longitude1,
                       m_route->latitude2 - m_route->latitude1) * kDegreesPerRadian + 360.0,
            360.0);
    }
}

bool AnimationConfiguredObject::IsValid() const
{
    return m_objectId != 0 && !m_animation.carriers.empty() &&
           (!m_route || m_routeLengthMeters > 0.0);
}

bool AnimationConfiguredObject::OwnsObject(AircraftId objectId) const
{
    return m_objectId == objectId;
}

std::optional<AnimationProximityTarget> AnimationConfiguredObject::ProximityTarget() const
{
    if (!m_route) return std::nullopt;
    return AnimationProximityTarget{m_currentLatitude, m_currentLongitude};
}

bool AnimationConfiguredObject::SetDistanceRankedActive(bool active, AnimationFrame &frame)
{
    if (!m_route || active == m_active) return false;
    m_active = active;
    if (!m_active) {
        QueueIdle(frame);
        return true;
    }
    m_animationStarted = frame.Now();
    m_movementUpdated = frame.Now();
    frame.FreezeObject(m_objectId);
    return true;
}

void AnimationConfiguredObject::Start(AnimationFrame &frame)
{
    frame.FreezeObject(m_objectId);
    m_animationStarted = frame.Now();
    m_movementUpdated = frame.Now();
    m_active = !m_route.has_value();
}

void AnimationConfiguredObject::Animate(AnimationFrame &frame)
{
    if (m_route && !m_active) return;
    const double elapsedSeconds = std::chrono::duration<double>(
        frame.Now() - m_animationStarted).count();

    std::optional<double> velocityBodyY;
    if (m_route) {
        const double movementSeconds = std::chrono::duration<double>(
            frame.Now() - m_movementUpdated).count();
        m_movementUpdated = frame.Now();
        m_routeDistanceMeters = std::fmod(
            m_routeDistanceMeters + movementSeconds * kWalkingSpeedMetersPerSecond,
            m_routeLengthMeters);
        const double firstSegmentLength = m_routeLengthMeters / 2.0;
        const bool outbound = m_routeDistanceMeters <= firstSegmentLength;
        const double segmentDistance = outbound
            ? m_routeDistanceMeters
            : m_routeDistanceMeters - firstSegmentLength;
        const double fraction = firstSegmentLength > 0.0
            ? segmentDistance / firstSegmentLength
            : 0.0;
        const double fromLatitude = outbound ? m_route->latitude1 : m_route->latitude2;
        const double fromLongitude = outbound ? m_route->longitude1 : m_route->longitude2;
        const double fromAltitude = outbound ? m_route->altitude1Feet : m_route->altitude2Feet;
        const double toLatitude = outbound ? m_route->latitude2 : m_route->latitude1;
        const double toLongitude = outbound ? m_route->longitude2 : m_route->longitude1;
        const double toAltitude = outbound ? m_route->altitude2Feet : m_route->altitude1Feet;
        m_currentLatitude = fromLatitude + (toLatitude - fromLatitude) * fraction;
        m_currentLongitude = fromLongitude + (toLongitude - fromLongitude) * fraction;
        m_currentAltitudeFeet = fromAltitude + (toAltitude - fromAltitude) * fraction;
        m_currentHeadingDegrees = std::fmod(
            std::atan2(toLongitude - fromLongitude, toLatitude - fromLatitude) *
                kDegreesPerRadian + 360.0,
            360.0);
    }

    for (const GroundServiceAnimationCarrier &carrier : m_animation.carriers) {
        const double value = CarrierValue(carrier, elapsedSeconds);
        if (m_route && EqualIgnoreCase(carrier.carrier, kVelocityBodyYCarrier)) {
            velocityBodyY = value;
        } else {
            frame.QueueCarrierUpdate({m_objectId, carrier.carrier, value});
        }
    }
    if (m_route && velocityBodyY) {
        frame.QueueMotionUpdate({m_objectId, m_currentLatitude, m_currentLongitude,
                                 m_currentAltitudeFeet, m_currentHeadingDegrees,
                                 *velocityBodyY});
    } else if (m_route) {
        frame.QueuePositionUpdate({m_objectId, m_currentLatitude, m_currentLongitude,
                                   m_currentAltitudeFeet, m_currentHeadingDegrees});
    }
}

void AnimationConfiguredObject::Cancel(AnimationFrame &frame) const
{
    frame.CancelObjectAnimation(m_objectId);
}

bool AnimationConfiguredObject::IsWalking() const
{
    return m_route.has_value() && m_active;
}

double AnimationConfiguredObject::CarrierValue(
    const GroundServiceAnimationCarrier &carrier, double elapsedSeconds) const
{
    const double first = m_animation.reversed ? carrier.lastFrame : carrier.firstFrame;
    const double last = m_animation.reversed ? carrier.firstFrame : carrier.lastFrame;
    const double range = last - first;
    if (std::abs(range) < 0.00001) return first;
    const double offset = std::fmod(elapsedSeconds * m_animation.framesPerSecond,
                                    std::abs(range));
    return first + (range > 0.0 ? offset : -offset);
}

void AnimationConfiguredObject::QueueIdle(AnimationFrame &frame) const
{
    bool wroteMotion = false;
    for (const GroundServiceAnimationCarrier &carrier : m_animation.carriers) {
        if (EqualIgnoreCase(carrier.carrier, kVelocityBodyYCarrier)) {
            frame.QueueMotionUpdate({m_objectId, m_currentLatitude, m_currentLongitude,
                                     m_currentAltitudeFeet, m_currentHeadingDegrees, 0.0});
            wroteMotion = true;
        } else {
            frame.QueueCarrierUpdate({m_objectId, carrier.carrier, 0.0});
        }
    }
    if (!wroteMotion) {
        frame.QueuePositionUpdate({m_objectId, m_currentLatitude, m_currentLongitude,
                                   m_currentAltitudeFeet, m_currentHeadingDegrees});
    }
}
} // namespace parking_services
