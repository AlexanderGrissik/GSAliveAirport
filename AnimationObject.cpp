#include "AnimationObject.h"

#include <utility>

namespace parking_services
{
AnimationFrame::AnimationFrame(SimConnectThread &simConnect) : m_simConnect(simConnect) {}

void AnimationFrame::Begin(std::chrono::steady_clock::time_point now)
{
    m_now = now;
    m_motionUpdates.clear();
    m_positionUpdates.clear();
    m_carrierUpdates.clear();
}

std::chrono::steady_clock::time_point AnimationFrame::Now() const
{
    return m_now;
}

void AnimationFrame::FreezeObject(AircraftId objectId)
{
    m_simConnect.FreezeObject(objectId);
}

void AnimationFrame::CancelObjectAnimation(AircraftId objectId)
{
    m_simConnect.CancelAnimationObject(objectId);
}

void AnimationFrame::QueueMotionUpdate(AnimationUpdate update)
{
    m_motionUpdates.push_back(std::move(update));
}

void AnimationFrame::QueuePositionUpdate(ObjectPositionUpdate update)
{
    m_positionUpdates.push_back(std::move(update));
}

void AnimationFrame::QueueCarrierUpdate(AnimationCarrierUpdate update)
{
    m_carrierUpdates.push_back(std::move(update));
}

void AnimationFrame::Flush()
{
    if (!m_motionUpdates.empty()) {
        m_simConnect.PublishAnimationUpdates(m_motionUpdates);
    }
    if (!m_positionUpdates.empty()) {
        m_simConnect.PublishObjectPositionUpdates(m_positionUpdates);
    }
    if (!m_carrierUpdates.empty()) {
        m_simConnect.PublishAnimationCarrierUpdates(m_carrierUpdates);
    }
}

std::optional<AnimationProximityTarget> AnimationObject::ProximityTarget() const
{
    return std::nullopt;
}

bool AnimationObject::SetDistanceRankedActive(bool, AnimationFrame &)
{
    return false;
}

bool AnimationObject::IsWalking() const
{
    return false;
}
} // namespace parking_services
