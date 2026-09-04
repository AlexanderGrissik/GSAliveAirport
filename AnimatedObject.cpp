#include "AnimatedObject.h"

#include "AnimationThread.h"
#include "SimObjectPositioning.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string_view>
#include <utility>

namespace parking_services
{
namespace
{
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

double Heading(const AnimationCoordinate &from, const AnimationCoordinate &to)
{
    return std::fmod(std::atan2(to.longitude - from.longitude,
                                to.latitude - from.latitude) *
                             kDegreesPerRadian +
                         360.0,
                     360.0);
}
} // namespace

AnimatedObject::AnimatedObject(
    AircraftId objectId, AnimationConfiguration animation,
    std::vector<AnimationCoordinate> movementCoordinates,
    double movementSpeedMetersPerSecond)
    : m_objectId(objectId), m_animation(std::move(animation)),
      m_movementCoordinates(std::move(movementCoordinates)),
      m_movementSpeedMetersPerSecond(movementSpeedMetersPerSecond)
{
    if (m_movementCoordinates.empty()) return;
    m_currentCoordinate = m_movementCoordinates.front();
    m_segmentLengths.reserve(m_movementCoordinates.size() - 1);
    for (std::size_t index = 1; index < m_movementCoordinates.size(); ++index) {
        const AnimationCoordinate &from = m_movementCoordinates[index - 1];
        const AnimationCoordinate &to = m_movementCoordinates[index];
        const double length = DistanceMeters(from.latitude, from.longitude,
                                             to.latitude, to.longitude);
        m_segmentLengths.push_back(length);
        m_oneWayLengthMeters += length;
    }
}

bool AnimatedObject::IsValid() const
{
    return m_objectId != 0 && !m_animation.carriers.empty() &&
           !m_movementCoordinates.empty() &&
           (m_movementCoordinates.size() == 1 ||
            (m_oneWayLengthMeters > 0.0 &&
             m_movementSpeedMetersPerSecond > 0.0));
}

std::optional<AnimationCoordinate> AnimatedObject::ProximityTarget() const
{
    if (!IsMoving()) return std::nullopt;
    return m_currentCoordinate;
}

bool AnimatedObject::SetDistanceRankedActive(bool active,
                                             AnimationThread &thread)
{
    if (!IsMoving() || active == m_active) return false;
    m_active = active;
    if (!m_active) {
        QueueIdle(thread);
        return true;
    }
    m_animationStarted = thread.Now();
    m_movementUpdated = thread.Now();
    thread.FreezeObject(m_objectId);
    return true;
}

void AnimatedObject::Start(AnimationThread &thread)
{
    thread.FreezeObject(m_objectId);
    m_animationStarted = thread.Now();
    m_movementUpdated = thread.Now();
    m_active = !IsMoving();
}

void AnimatedObject::Animate(AnimationThread &thread)
{
    if (IsMoving() && !m_active) return;
    const double elapsedSeconds =
        std::chrono::duration<double>(thread.Now() - m_animationStarted).count();

    std::optional<double> velocityBodyY;
    if (IsMoving()) {
        const double movementSeconds =
            std::chrono::duration<double>(thread.Now() - m_movementUpdated).count();
        m_movementUpdated = thread.Now();
        AdvanceMovement(movementSeconds);
    }

    for (const AnimationCarrier &carrier : m_animation.carriers) {
        const double value = CarrierValue(carrier, elapsedSeconds);
        if (IsMoving() && EqualIgnoreCase(carrier.name, kVelocityBodyYCarrier)) {
            velocityBodyY = value;
        } else {
            thread.QueueCarrierUpdate(m_objectId, carrier.name, value);
        }
    }
    if (IsMoving() && velocityBodyY) {
        thread.QueueMotionUpdate(m_objectId, m_currentCoordinate, *velocityBodyY);
    } else if (IsMoving()) {
        thread.QueuePositionUpdate(m_objectId, m_currentCoordinate);
    }
}

void AnimatedObject::Cancel(AnimationThread &thread) const
{
    thread.CancelObjectAnimation(m_objectId);
}

bool AnimatedObject::IsMoving() const
{
    return m_movementCoordinates.size() > 1;
}

double AnimatedObject::CarrierValue(
    const AnimationCarrier &carrier, double elapsedSeconds) const
{
    const double first = m_animation.reversed ? carrier.lastFrame : carrier.firstFrame;
    const double last = m_animation.reversed ? carrier.firstFrame : carrier.lastFrame;
    const double range = last - first;
    if (std::abs(range) < 0.00001) return first;
    const double offset = std::fmod(elapsedSeconds * m_animation.framesPerSecond,
                                    std::abs(range));
    return first + (range > 0.0 ? offset : -offset);
}

void AnimatedObject::AdvanceMovement(double movementSeconds)
{
    const double fullLength = m_oneWayLengthMeters * 2.0;
    m_routeDistanceMeters = std::fmod(
        m_routeDistanceMeters +
            movementSeconds * m_movementSpeedMetersPerSecond,
        fullLength);
    const bool forward = m_routeDistanceMeters <= m_oneWayLengthMeters;
    const double oneWayDistance = forward
                                      ? m_routeDistanceMeters
                                      : fullLength - m_routeDistanceMeters;

    double segmentStart = 0.0;
    std::size_t segment = m_segmentLengths.size() - 1;
    for (std::size_t index = 0; index < m_segmentLengths.size(); ++index) {
        if (oneWayDistance <= segmentStart + m_segmentLengths[index]) {
            segment = index;
            break;
        }
        segmentStart += m_segmentLengths[index];
    }

    const double length = m_segmentLengths[segment];
    const double fraction = length > 0.0
                                ? std::clamp((oneWayDistance - segmentStart) / length,
                                             0.0, 1.0)
                                : 0.0;
    const AnimationCoordinate &first = m_movementCoordinates[segment];
    const AnimationCoordinate &second = m_movementCoordinates[segment + 1];
    m_currentCoordinate.latitude =
        first.latitude + (second.latitude - first.latitude) * fraction;
    m_currentCoordinate.longitude =
        first.longitude + (second.longitude - first.longitude) * fraction;
    m_currentCoordinate.altitudeFeet =
        first.altitudeFeet + (second.altitudeFeet - first.altitudeFeet) * fraction;
    m_currentCoordinate.headingDegrees =
        forward ? Heading(first, second) : Heading(second, first);
}

void AnimatedObject::QueueIdle(AnimationThread &thread) const
{
    bool wroteMotion = false;
    for (const AnimationCarrier &carrier : m_animation.carriers) {
        if (EqualIgnoreCase(carrier.name, kVelocityBodyYCarrier)) {
            thread.QueueMotionUpdate(m_objectId, m_currentCoordinate, 0.0);
            wroteMotion = true;
        } else {
            thread.QueueCarrierUpdate(m_objectId, carrier.name, 0.0);
        }
    }
    if (!wroteMotion) {
        thread.QueuePositionUpdate(m_objectId, m_currentCoordinate);
    }
}
} // namespace parking_services
