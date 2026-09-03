#include "AnimationObject.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace parking_services
{
namespace
{
constexpr SIMCONNECT_DATA_DEFINITION_ID kAnimationDefinition = 3;
constexpr SIMCONNECT_DATA_DEFINITION_ID kPositionDefinition = 6;
constexpr SIMCONNECT_CLIENT_EVENT_ID kFreezeLatitudeLongitudeEvent = 5;
constexpr SIMCONNECT_CLIENT_EVENT_ID kFreezeAltitudeEvent = 6;
constexpr SIMCONNECT_CLIENT_EVENT_ID kFreezeAttitudeEvent = 7;

struct AnimationWireData
{
    double latitude{};
    double longitude{};
    double altitudeFeet{};
    double headingDegrees{};
    double velocityBodyYMetersPerSecond{};
};

static_assert(sizeof(AnimationWireData) == 40);
} // namespace

AnimationFrame::AnimationFrame(ISimConnectHandler &simConnect)
    : m_simConnect(simConnect)
{
}

void AnimationFrame::InitializeSimConnect()
{
    CollectFinishedRequests();
    m_simConnect.AddDatum(kAnimationDefinition, "PLANE LATITUDE", "degrees",
                          SIMCONNECT_DATATYPE_FLOAT64, NewCommandRequest());
    m_simConnect.AddDatum(kAnimationDefinition, "PLANE LONGITUDE", "degrees",
                          SIMCONNECT_DATATYPE_FLOAT64, NewCommandRequest());
    m_simConnect.AddDatum(kAnimationDefinition, "PLANE ALTITUDE", "feet",
                          SIMCONNECT_DATATYPE_FLOAT64, NewCommandRequest());
    m_simConnect.AddDatum(kAnimationDefinition, "PLANE HEADING DEGREES TRUE",
                          "degrees", SIMCONNECT_DATATYPE_FLOAT64,
                          NewCommandRequest());
    m_simConnect.AddDatum(kAnimationDefinition, "VELOCITY BODY Y",
                          "meters per second", SIMCONNECT_DATATYPE_FLOAT64,
                          NewCommandRequest());
    m_simConnect.AddDatum(kPositionDefinition, "Initial Position", "",
                          SIMCONNECT_DATATYPE_INITPOSITION, NewCommandRequest());
    m_simConnect.MapClientEvent(kFreezeLatitudeLongitudeEvent,
                                "FREEZE_LATITUDE_LONGITUDE_SET",
                                NewCommandRequest());
    m_simConnect.MapClientEvent(kFreezeAltitudeEvent, "FREEZE_ALTITUDE_SET",
                                NewCommandRequest());
    m_simConnect.MapClientEvent(kFreezeAttitudeEvent, "FREEZE_ATTITUDE_SET",
                                NewCommandRequest());
}

GSReqCommand &AnimationFrame::NewCommandRequest()
{
    auto request = std::make_unique<GSReqCommand>();
    GSReqCommand &reference = *request;
    m_commandRequests.push_back(std::move(request));
    return reference;
}

void AnimationFrame::CollectFinishedRequests()
{
    std::erase_if(m_commandRequests, [](const auto &request) {
        return request->IsFinished();
    });
}

void AnimationFrame::Begin(std::chrono::steady_clock::time_point now)
{
    CollectFinishedRequests();
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
    m_simConnect.TransmitEvent(objectId, kFreezeLatitudeLongitudeEvent, 1,
                               NewCommandRequest());
    m_simConnect.TransmitEvent(objectId, kFreezeAltitudeEvent, 1,
                               NewCommandRequest());
    m_simConnect.TransmitEvent(objectId, kFreezeAttitudeEvent, 1,
                               NewCommandRequest());
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
        m_simConnect.SetObjectData(kAnimationDefinition, objectId, 0,
                                   sizeof(data), &data, NewCommandRequest());
    }

    for (const auto &[objectId, update] : m_positionUpdates) {
        SIMCONNECT_DATA_INITPOSITION position{};
        position.Latitude = update.latitude;
        position.Longitude = update.longitude;
        position.Altitude = update.altitudeFeet;
        position.Heading = update.headingDegrees;
        position.OnGround = 1;
        m_simConnect.SetObjectData(kPositionDefinition, objectId, 0,
                                   sizeof(position), &position, NewCommandRequest());
    }

    for (const auto &[key, update] : m_carrierUpdates) {
        static_cast<void>(key);
        auto definition = m_carrierDefinitions.find(update.carrier);
        if (definition == m_carrierDefinitions.end()) {
            const auto definitionId = static_cast<SIMCONNECT_DATA_DEFINITION_ID>(
                m_nextCarrierDefinitionId++);
            definition = m_carrierDefinitions.emplace(update.carrier, definitionId).first;
            m_simConnect.AddDatum(
                definitionId, update.carrier,
                update.carrier == "VELOCITY BODY Y" ? "meters per second" : "number",
                SIMCONNECT_DATATYPE_FLOAT64, NewCommandRequest());
        }
        m_simConnect.SetObjectData(definition->second, update.objectId, 0,
                                   sizeof(update.value), &update.value,
                                   NewCommandRequest());
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
