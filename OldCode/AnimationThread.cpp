#include "AnimationThread.h"

#include "AircraftTrackerThread.h"
#include "GSCommon.h"
#include "SimObjectPositioning.h"

#include <algorithm>
#include <utility>

namespace parking_services
{
using namespace std::chrono_literals;

namespace
{
constexpr auto kUpdateInterval = std::chrono::microseconds(33'333);
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

AnimationThread::AnimationThread(ISimConnectHandler &simConnect,
                                 AircraftTrackerThread &aircraftTracker)
    : m_simConnect(simConnect), m_aircraftTracker(aircraftTracker)
{
}

AnimationThread::~AnimationThread()
{
    Stop();
}

void AnimationThread::Start()
{
    if (m_thread.joinable()) return;
    m_thread = std::jthread(&AnimationThread::AnimationLoop, this);
}

void AnimationThread::Stop()
{
    if (!m_thread.joinable()) return;
    m_thread.request_stop();
    m_wake.notify_all();
    m_thread.join();
}

void AnimationThread::InitializeSimConnect()
{
    Post([this] {
        CollectFinishedRequests();
        m_carrierDefinitions.clear();
        m_nextCarrierDefinitionId = 100;
        m_simConnect.AddDatum(kAnimationDefinition, "PLANE LATITUDE", "degrees",
                              SIMCONNECT_DATATYPE_FLOAT64, NewCommandRequest());
        m_simConnect.AddDatum(kAnimationDefinition, "PLANE LONGITUDE", "degrees",
                              SIMCONNECT_DATATYPE_FLOAT64, NewCommandRequest());
        m_simConnect.AddDatum(kAnimationDefinition, "PLANE ALTITUDE", "feet",
                              SIMCONNECT_DATATYPE_FLOAT64, NewCommandRequest());
        m_simConnect.AddDatum(kAnimationDefinition,
                              "PLANE HEADING DEGREES TRUE", "degrees",
                              SIMCONNECT_DATATYPE_FLOAT64, NewCommandRequest());
        m_simConnect.AddDatum(kAnimationDefinition, "VELOCITY BODY Y",
                              "meters per second", SIMCONNECT_DATATYPE_FLOAT64,
                              NewCommandRequest());
        m_simConnect.AddDatum(kPositionDefinition, "Initial Position", "",
                              SIMCONNECT_DATATYPE_INITPOSITION,
                              NewCommandRequest());
        m_simConnect.MapClientEvent(kFreezeLatitudeLongitudeEvent,
                                    "FREEZE_LATITUDE_LONGITUDE_SET",
                                    NewCommandRequest());
        m_simConnect.MapClientEvent(kFreezeAltitudeEvent,
                                    "FREEZE_ALTITUDE_SET",
                                    NewCommandRequest());
        m_simConnect.MapClientEvent(kFreezeAttitudeEvent,
                                    "FREEZE_ATTITUDE_SET",
                                    NewCommandRequest());
    });
}

void AnimationThread::AddObject(std::unique_ptr<AnimatedObject> object)
{
    Post([this, object = std::move(object)]() mutable {
        AddObjectInternal(std::move(object));
    });
}

void AnimationThread::RemoveObject(AircraftId objectId)
{
    Post([this, objectId] { RemoveObjectInternal(objectId); });
}

void AnimationThread::Reset()
{
    Post([this] { ResetInternal(); });
}

AnimationStatus AnimationThread::Status() const
{
    std::scoped_lock lock(m_statusMutex);
    return m_status;
}

void AnimationThread::AnimationLoop(std::stop_token stopToken,
                                    AnimationThread *self)
{
    self->RunLoop(stopToken);
}

void AnimationThread::RunLoop(std::stop_token stopToken)
{
    m_now = std::chrono::steady_clock::now();
    auto nextUpdate = m_now;
    while (!stopToken.stop_requested()) {
        ProcessCommands();
        const auto now = std::chrono::steady_clock::now();
        if (now >= nextUpdate) {
            Tick(now);
            nextUpdate = now + kUpdateInterval;
        }
        std::unique_lock lock(m_commandMutex);
        m_wake.wait_until(lock, stopToken, nextUpdate,
                          [this] { return !m_commands.empty(); });
    }
    ProcessCommands();
    ResetInternal();
}

void AnimationThread::ProcessCommands()
{
    std::deque<std::packaged_task<void()>> commands;
    {
        std::scoped_lock lock(m_commandMutex);
        commands.swap(m_commands);
    }
    for (auto &command : commands) command();
}

void AnimationThread::Tick(std::chrono::steady_clock::time_point now)
{
    CollectFinishedRequests();
    m_now = now;
    m_motionUpdates.clear();
    m_positionUpdates.clear();
    m_carrierUpdates.clear();

    ApproximateUserPosition userPosition{};
    const bool hasUserPosition =
        m_aircraftTracker.TryGetApproximateUserPosition(userPosition);

    m_distanceRanking.clear();
    if (hasUserPosition) {
        if (m_distanceRanking.capacity() < m_animations.size()) {
            m_distanceRanking.reserve(m_animations.size());
        }
        for (const auto &[animationId, animation] : m_animations) {
            const auto target = animation->ProximityTarget();
            if (!target) continue;
            m_distanceRanking.push_back(
                {animationId,
                 DistanceMeters(userPosition.latitude, userPosition.longitude,
                                target->latitude, target->longitude)});
        }
        std::ranges::sort(m_distanceRanking,
                          [](const AnimationDistance &left,
                             const AnimationDistance &right) {
                              if (left.meters != right.meters) {
                                  return left.meters < right.meters;
                              }
                              return left.animationId < right.animationId;
                          });
    }

    const std::size_t activeCount =
        (std::min)(MaximumMovingObjects, m_distanceRanking.size());
    bool selectionChanged = false;
    for (auto &[animationId, animation] : m_animations) {
        const bool active = std::ranges::any_of(
            m_distanceRanking.begin(), m_distanceRanking.begin() + activeCount,
            [animationId](const AnimationDistance &entry) {
                return entry.animationId == animationId;
            });
        selectionChanged |= animation->SetDistanceRankedActive(active, *this);
    }

    for (auto &[animationId, animation] : m_animations) {
        static_cast<void>(animationId);
        animation->Animate(*this);
    }
    Flush();
    if (selectionChanged) PublishStatus();
}

void AnimationThread::AddObjectInternal(std::unique_ptr<AnimatedObject> object)
{
    if (!object || !object->IsValid()) {
        GSLog("Could not register an invalid animated object.");
        return;
    }
    const AircraftId objectId = object->ObjectId();
    if (ContainsAnimationObject(objectId)) return;
    const bool moving = object->IsMoving();
    object->Start(*this);
    m_animations.emplace(objectId, std::move(object));
    GSLog("Registered animation for ObjectID " + std::to_string(objectId) +
          (moving ? " with a movement path." : "."));
    PublishStatus();
}

void AnimationThread::RemoveObjectInternal(AircraftId objectId)
{
    const auto animation = m_animations.find(objectId);
    if (animation == m_animations.end()) return;
    animation->second->Cancel(*this);
    m_animations.erase(animation);
    PublishStatus();
}

void AnimationThread::ResetInternal()
{
    for (const auto &[animationId, animation] : m_animations) {
        static_cast<void>(animationId);
        animation->Cancel(*this);
    }
    m_animations.clear();
    PublishStatus();
}

void AnimationThread::PublishStatus()
{
    const std::size_t movingObjects = static_cast<std::size_t>(
        std::ranges::count_if(m_animations, [](const auto &entry) {
            return entry.second->IsMoving() && entry.second->IsActive();
        }));
    std::scoped_lock lock(m_statusMutex);
    m_status = {movingObjects};
}

bool AnimationThread::ContainsAnimationObject(AircraftId objectId) const
{
    return m_animations.contains(objectId);
}

std::chrono::steady_clock::time_point AnimationThread::Now() const
{
    return m_now;
}

void AnimationThread::FreezeObject(AircraftId objectId)
{
    m_simConnect.TransmitEvent(objectId, kFreezeLatitudeLongitudeEvent, 1,
                               NewCommandRequest());
    m_simConnect.TransmitEvent(objectId, kFreezeAltitudeEvent, 1,
                               NewCommandRequest());
    m_simConnect.TransmitEvent(objectId, kFreezeAttitudeEvent, 1,
                               NewCommandRequest());
}

void AnimationThread::CancelObjectAnimation(AircraftId objectId)
{
    m_motionUpdates.erase(objectId);
    m_positionUpdates.erase(objectId);
    std::erase_if(m_carrierUpdates, [objectId](const auto &entry) {
        return entry.second.objectId == objectId;
    });
}

void AnimationThread::QueueMotionUpdate(
    AircraftId objectId, const AnimationCoordinate &coordinate,
    double velocityBodyYMetersPerSecond)
{
    m_motionUpdates[objectId] =
        {objectId, coordinate, velocityBodyYMetersPerSecond};
}

void AnimationThread::QueuePositionUpdate(
    AircraftId objectId, const AnimationCoordinate &coordinate)
{
    m_positionUpdates[objectId] = {objectId, coordinate};
}

void AnimationThread::QueueCarrierUpdate(AircraftId objectId,
                                         std::string carrier, double value)
{
    const auto key = std::pair{objectId, carrier};
    m_carrierUpdates[key] = {objectId, std::move(carrier), value};
}

void AnimationThread::Flush()
{
    for (const auto &[objectId, update] : m_motionUpdates) {
        const AnimationWireData data{
            update.coordinate.latitude, update.coordinate.longitude,
            update.coordinate.altitudeFeet, update.coordinate.headingDegrees,
            update.velocityBodyYMetersPerSecond};
        m_simConnect.SetObjectData(kAnimationDefinition, objectId, 0,
                                   sizeof(data), &data, NewCommandRequest());
    }

    for (const auto &[objectId, update] : m_positionUpdates) {
        SIMCONNECT_DATA_INITPOSITION position{};
        position.Latitude = update.coordinate.latitude;
        position.Longitude = update.coordinate.longitude;
        position.Altitude = update.coordinate.altitudeFeet;
        position.Heading = update.coordinate.headingDegrees;
        position.OnGround = 1;
        m_simConnect.SetObjectData(kPositionDefinition, objectId, 0,
                                   sizeof(position), &position,
                                   NewCommandRequest());
    }

    for (const auto &[key, update] : m_carrierUpdates) {
        static_cast<void>(key);
        auto definition = m_carrierDefinitions.find(update.carrier);
        if (definition == m_carrierDefinitions.end()) {
            const auto definitionId = static_cast<SIMCONNECT_DATA_DEFINITION_ID>(
                m_nextCarrierDefinitionId++);
            definition =
                m_carrierDefinitions.emplace(update.carrier, definitionId).first;
            m_simConnect.AddDatum(
                definitionId, update.carrier,
                update.carrier == "VELOCITY BODY Y" ? "meters per second"
                                                    : "number",
                SIMCONNECT_DATATYPE_FLOAT64, NewCommandRequest());
        }
        m_simConnect.SetObjectData(definition->second, update.objectId, 0,
                                   sizeof(update.value), &update.value,
                                   NewCommandRequest());
    }
}

GSReqCommand &AnimationThread::NewCommandRequest()
{
    auto request = std::make_unique<GSReqCommand>();
    GSReqCommand &reference = *request;
    m_commandRequests.push_back(std::move(request));
    return reference;
}

void AnimationThread::CollectFinishedRequests()
{
    std::erase_if(m_commandRequests, [](const auto &request) {
        return request->IsFinished();
    });
}
} // namespace parking_services
