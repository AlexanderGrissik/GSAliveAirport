#include "AnimationObject.h"

#include "SimConnectIds.h"
#include "GSRequests/GSReqCommand.h"

#include <algorithm>
#include <utility>

namespace parking_services
{
namespace
{
struct AnimationWireData
{
    double latitude{};
    double longitude{};
    double altitudeFeet{};
    double headingDegrees{};
    double velocityBodyYMetersPerSecond{};
};

static_assert(sizeof(AnimationWireData) == 40);

std::shared_ptr<GSReqCommand> CommandRequest()
{
    return std::make_shared<GSReqCommand>();
}
} // namespace

AnimationFrame::AnimationFrame(ISimConnectHandler &simConnect)
    : m_simConnect(simConnect)
{
}

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
    m_simConnect.TransmitEvent(objectId, EventFreezeLatitudeLongitude, 1,
                               CommandRequest());
    m_simConnect.TransmitEvent(objectId, EventFreezeAltitude, 1,
                               CommandRequest());
    m_simConnect.TransmitEvent(objectId, EventFreezeAttitude, 1,
                               CommandRequest());
}

void AnimationFrame::CancelObjectAnimation(AircraftId objectId)
{
    m_motionUpdates.erase(objectId);
    m_positionUpdates.erase(objectId);
    std::erase_if(m_carrierUpdates, [objectId](const auto &entry) {
        return entry.second.objectId == objectId;
    });
}

void AnimationFrame::QueueMotionUpdate(AnimationUpdate update)
{
    m_motionUpdates[update.objectId] = std::move(update);
}

void AnimationFrame::QueuePositionUpdate(ObjectPositionUpdate update)
{
    m_positionUpdates[update.objectId] = std::move(update);
}

void AnimationFrame::QueueCarrierUpdate(AnimationCarrierUpdate update)
{
    const auto key = std::pair{update.objectId, update.carrier};
    m_carrierUpdates[key] = std::move(update);
}

void AnimationFrame::Flush()
{
    for (const auto &[objectId, update] : m_motionUpdates) {
        const AnimationWireData data{update.latitude, update.longitude,
                                     update.altitudeFeet, update.headingDegrees,
                                     update.velocityBodyYMetersPerSecond};
        m_simConnect.SetObjectData(DefinitionAnimationUpdate, objectId, 0,
                                   sizeof(data), &data, CommandRequest());
    }

    for (const auto &[objectId, update] : m_positionUpdates) {
        SIMCONNECT_DATA_INITPOSITION position{};
        position.Latitude = update.latitude;
        position.Longitude = update.longitude;
        position.Altitude = update.altitudeFeet;
        position.Heading = update.headingDegrees;
        position.OnGround = 1;
        m_simConnect.SetObjectData(DefinitionObjectPosition, objectId, 0,
                                   sizeof(position), &position, CommandRequest());
    }

    for (const auto &[key, update] : m_carrierUpdates) {
        static_cast<void>(key);
        auto definition = m_carrierDefinitions.find(update.carrier);
        if (definition == m_carrierDefinitions.end()) {
            const auto definitionId = static_cast<SIMCONNECT_DATA_DEFINITION_ID>(
                m_nextCarrierDefinitionId++);
            definition = m_carrierDefinitions.emplace(update.carrier, definitionId).first;
            m_simConnect.AddToDataDefinition(
                definitionId, update.carrier,
                update.carrier == "VELOCITY BODY Y" ? "meters per second" : "number",
                SIMCONNECT_DATATYPE_FLOAT64, CommandRequest());
        }
        m_simConnect.SetObjectData(definition->second, update.objectId, 0,
                                   sizeof(update.value), &update.value,
                                   CommandRequest());
    }
}

void AnimationFrame::ResetDefinitions()
{
    m_carrierDefinitions.clear();
    m_nextCarrierDefinitionId = 100;
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
