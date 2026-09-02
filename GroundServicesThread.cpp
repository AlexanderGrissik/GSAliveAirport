#include "GroundServicesThread.h"

#include "AircraftTrackerThread.h"
#include "SimConnectThread.h"
#include "SimObjectPositioning.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace parking_services
{
using namespace std::chrono_literals;

namespace
{
constexpr double kSpawnSpeedKnots = 1.0;
constexpr double kRemovalSpeedKnots = 2.0;
constexpr double kFeetToMeters = 0.3048;
constexpr double kCargoDoorClearanceMeters = 0.5;
constexpr double kRadiansToDegrees = 180.0 / 3.14159265358979323846;

double NormalizeDegrees(double degrees)
{
    return std::fmod(degrees + 360.0, 360.0);
}
} // namespace

GroundServicesThread::GroundServicesThread(SimConnectThread &simConnect,
                                           AircraftTrackerThread &aircraftTracker,
                                           AnimationThread &animation,
                                           GroundServicesConfig &configuration,
                                           LogSink log)
    : m_simConnect(simConnect), m_aircraftTracker(aircraftTracker),
      m_animation(animation), m_configuration(configuration), m_log(std::move(log))
{
    m_simConnect.SubscribeConnection(
        [this](bool connected) { Post([this, connected] { HandleConnection(connected); }); });
    m_simConnect.SubscribeObjectRemoved(
        [this](DWORD objectId) { Post([this, objectId] { HandleObjectRemoved(objectId); }); });
    m_thread = std::jthread(&GroundServicesThread::GroundServicesLoop, this);
}

GroundServicesThread::~GroundServicesThread()
{
    Stop();
}

void GroundServicesThread::Stop()
{
    if (!m_thread.joinable()) return;
    m_stopping.store(true);
    m_thread.request_stop();
    m_wake.notify_all();
    m_thread.join();
}

void GroundServicesThread::Reset()
{
    Post([this] {
        RemoveAllServicesInternal();
        m_log("Reset ground-service state and requested removal of all created objects.");
    });
}

GroundServicesStatus GroundServicesThread::Status() const
{
    std::scoped_lock lock(m_statusMutex);
    return m_status;
}

void GroundServicesThread::GroundServicesLoop(std::stop_token stopToken,
                                               GroundServicesThread *self)
{
    self->RunLoop(stopToken);
}

void GroundServicesThread::RunLoop(std::stop_token stopToken)
{
    auto nextDecision = std::chrono::steady_clock::now();
    while (!stopToken.stop_requested()) {
        ProcessCommands();
        const auto now = std::chrono::steady_clock::now();
        if (m_connected) MaintainCargoDoorAlignments(now);
        if (m_connected && now >= nextDecision) {
            EvaluateTrackedAircraft();
            nextDecision = now + 10s;
        }
        std::unique_lock lock(m_commandMutex);
        m_wake.wait_for(lock, stopToken, 100ms, [this] { return !m_commands.empty(); });
    }
    ProcessCommands();
    RemoveAllServicesInternal();
}

void GroundServicesThread::Post(std::function<void()> command)
{
    {
        std::scoped_lock lock(m_commandMutex);
        m_commands.push_back(std::move(command));
    }
    m_wake.notify_all();
}

void GroundServicesThread::ProcessCommands()
{
    std::deque<std::function<void()>> commands;
    {
        std::scoped_lock lock(m_commandMutex);
        commands.swap(m_commands);
    }
    for (auto &command : commands) command();
}

void GroundServicesThread::MaintainCargoDoorAlignments(
    std::chrono::steady_clock::time_point now)
{
    for (auto &[objectId, alignment] : m_pendingCargoDoorAlignments) {
        if (alignment.geometryRequested || now < alignment.geometryRequestDue) continue;
        alignment.geometryRequested = true;
        m_simConnect.RequestBaggageLoaderGeometry(
            objectId, [this, objectId](BaggageLoaderGeometry geometry) {
                Post([this, objectId, geometry] {
                    CompleteCargoDoorAlignment(objectId, geometry);
                });
            });
    }
}

void GroundServicesThread::CompleteCargoDoorAlignment(
    AircraftId objectId, BaggageLoaderGeometry geometry)
{
    const auto pending = m_pendingCargoDoorAlignments.find(objectId);
    if (pending == m_pendingCargoDoorAlignments.end()) return;
    PendingCargoDoorAlignment &alignment = pending->second;
    alignment.geometryRequested = false;

    const auto finishAtInitialPosition = [this, objectId, &alignment, pending] {
        PendingCreate created{};
        created.aircraft = alignment.aircraft;
        created.object = alignment.object;
        created.pose = alignment.initialPose;
        created.route = alignment.route;
        created.parentObjectId = alignment.parentObjectId;
        m_pendingCargoDoorAlignments.erase(pending);
        FinalizeCreatedObject(objectId, created, created.pose);
    };

    if (!geometry.succeeded || !std::isfinite(geometry.angleCurrentDegrees) ||
        !std::isfinite(geometry.endRampYMeters) || !std::isfinite(geometry.endRampZMeters) ||
        !std::isfinite(geometry.pivotYMeters) || !std::isfinite(geometry.pivotZMeters)) {
        m_log("Could not read cargo-door ramp geometry for ObjectID " +
              std::to_string(objectId) + "; keeping the configured object at the door point.");
        finishAtInitialPosition();
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (alignment.stage == PendingCargoDoorAlignment::Stage::MeasureInitialGeometry) {
        const double rampLength = std::hypot(geometry.endRampYMeters - geometry.pivotYMeters,
                                             geometry.endRampZMeters - geometry.pivotZMeters);
        if (rampLength < 0.01) {
            m_log("MSFS returned no usable cargo-door ramp geometry for ObjectID " +
                  std::to_string(objectId) + "; keeping the configured object at the door point.");
            finishAtInitialPosition();
            return;
        }
        const double currentPhase = std::atan2(geometry.endRampYMeters - geometry.pivotYMeters,
                                               geometry.endRampZMeters - geometry.pivotZMeters);
        const double desiredPhase = std::asin(std::clamp(
            (alignment.cargoDoor.cargoHeightMeters - geometry.pivotYMeters) / rampLength,
            -1.0, 1.0));
        alignment.rampAngleDegrees = std::clamp(
            geometry.angleCurrentDegrees + (desiredPhase - currentPhase) * kRadiansToDegrees,
            0.0, 90.0);
        alignment.stage = PendingCargoDoorAlignment::Stage::WaitForRampTarget;
        alignment.geometryRequestDue = now + 500ms;
        m_simConnect.SetBaggageLoaderRampTarget(objectId, alignment.rampAngleDegrees);
        return;
    }

    if (std::abs(geometry.angleCurrentDegrees - alignment.rampAngleDegrees) > 0.2) {
        alignment.geometryRequestDue = now + 500ms;
        return;
    }

    const double headingRadians = alignment.cargoDoor.modelRelativeHeadingDegrees *
        3.14159265358979323846 / 180.0;
    const double rampDistanceMeters = geometry.endRampZMeters + kCargoDoorClearanceMeters;
    const double forwardMeters = alignment.cargoDoor.cargoForwardMeters -
        std::cos(headingRadians) * rampDistanceMeters;
    const double rightMeters = alignment.cargoDoor.cargoRightMeters -
        std::sin(headingRadians) * rampDistanceMeters;
    SpawnPose actualPose = RelativeToAircraft(alignment.aircraft, rightMeters, forwardMeters,
                                              false);
    actualPose.headingDegrees = alignment.initialPose.headingDegrees;
    m_simConnect.SetObjectPosition(objectId, ToInitialPosition(actualPose));

    PendingCreate created{};
    created.aircraft = alignment.aircraft;
    created.object = alignment.object;
    created.pose = actualPose;
    created.route = alignment.route;
    created.parentObjectId = alignment.parentObjectId;
    m_pendingCargoDoorAlignments.erase(pending);
    FinalizeCreatedObject(objectId, created, actualPose);
    m_log("Aligned configured cargo-door object ObjectID " + std::to_string(objectId) +
          " with " + std::to_string(kCargoDoorClearanceMeters) + " m door clearance.");
}

void GroundServicesThread::EvaluateTrackedAircraft()
{
    ResolveConfigurationIfAvailable();
    m_aircraftTracker.FillTrackedAircraftSnapshot(m_aircraftSnapshotBuffer);
    std::set<AircraftId> present;
    for (const AircraftSnapshot &aircraft : m_aircraftSnapshotBuffer) {
        present.insert(aircraft.objectId);
        switch (Decide(aircraft)) {
        case GroundServicesDecision::Add:
            EnsureAutomaticServices(aircraft);
            break;
        case GroundServicesDecision::Remove:
            RemoveForAircraft(aircraft.objectId);
            break;
        case GroundServicesDecision::Keep:
            break;
        }
    }

    std::set<AircraftId> owned;
    owned.insert(m_configuredAircraft.begin(), m_configuredAircraft.end());
    for (const auto &[aircraftId, objects] : m_objectsByAircraft) {
        static_cast<void>(objects);
        owned.insert(aircraftId);
    }
    for (const auto &[token, pending] : m_pendingCreates) {
        static_cast<void>(token);
        owned.insert(pending.aircraft.objectId);
    }
    for (const auto &[objectId, alignment] : m_pendingCargoDoorAlignments) {
        static_cast<void>(objectId);
        owned.insert(alignment.aircraft.objectId);
    }
    for (const AircraftId aircraftId : owned) {
        if (!present.contains(aircraftId)) RemoveForAircraft(aircraftId);
    }
}

void GroundServicesThread::ResolveConfigurationIfAvailable()
{
    if (!m_configuration.IsLoaded() || m_configuration.IsResolved() ||
        !m_simConnect.FillAvailableSimObjectTitles(m_catalogTitleBuffer)) {
        return;
    }
    m_configuration.Resolve(m_catalogTitleBuffer, m_configurationMessageBuffer);
    for (const std::string &message : m_configurationMessageBuffer) m_log(message);
}

GroundServicesDecision GroundServicesThread::Decide(const AircraftSnapshot &aircraft)
{
    const bool taxi = NormalizeTrafficState(aircraft.trafficState) == "simple taxi";
    if (taxi || std::abs(aircraft.groundSpeedKnots) > kRemovalSpeedKnots) {
        return GroundServicesDecision::Remove;
    }
    if (!aircraft.isUser && aircraft.onGround && aircraft.lightNav && !taxi &&
        std::abs(aircraft.groundSpeedKnots) < kSpawnSpeedKnots) {
        return GroundServicesDecision::Add;
    }
    return GroundServicesDecision::Keep;
}

void GroundServicesThread::EnsureAutomaticServices(const AircraftSnapshot &aircraft)
{
    if (!m_connected || !m_configuration.IsResolved() ||
        m_configuredAircraft.contains(aircraft.objectId) ||
        m_objectsByAircraft.contains(aircraft.objectId) || HasPendingFor(aircraft.objectId)) {
        return;
    }
    const auto category = ClassifyAircraftSize(aircraft.wingSpanMeters);
    if (!category) return;

    m_configuration.FillRequests(*category, m_random, m_serviceRequestBuffer);
    m_configuredAircraft.insert(aircraft.objectId);
    for (const GroundServiceRequest &request : m_serviceRequestBuffer) {
        QueueService(aircraft, request);
    }
    m_log("Applied " + std::to_string(m_serviceRequestBuffer.size()) +
          " configured " + std::string(AircraftSizeCategoryName(*category)) +
          " service request(s) to aircraft " + std::to_string(aircraft.objectId) +
          " (wingspan " + std::to_string(aircraft.wingSpanMeters) + " m).");
    PublishStatus();
}

void GroundServicesThread::QueueService(const AircraftSnapshot &aircraft,
                                        const GroundServiceRequest &request)
{
    PendingCreate pending{};
    pending.aircraft = aircraft;
    pending.object = request.object;
    switch (request.location.kind) {
    case GroundServiceLocationKind::Static:
        pending.pose = RelativeToAircraft(aircraft, request.location.relX1,
                                          request.location.relY1,
                                          request.location.faceAircraft);
        break;
    case GroundServiceLocationKind::Route: {
        pending.pose = RelativeToAircraft(aircraft, request.location.relX1,
                                          request.location.relY1, false);
        const SpawnPose endpoint = RelativeToAircraft(aircraft, request.location.relX2,
                                                      request.location.relY2, false);
        pending.route = {pending.pose.latitude, pending.pose.longitude,
                         pending.pose.altitudeFeet, endpoint.latitude, endpoint.longitude,
                         endpoint.altitudeFeet};
        break;
    }
    case GroundServiceLocationKind::CargoDoorRightFront:
    case GroundServiceLocationKind::CargoDoorRightBack: {
        const auto &connection = request.location.kind ==
                GroundServiceLocationKind::CargoDoorRightFront
            ? aircraft.cargoDoorRightFront
            : aircraft.cargoDoorRightBack;
        if (!connection) {
            m_log("Skipped " + request.object.family + " for aircraft " +
                  std::to_string(aircraft.objectId) + ": MSFS reported no matching right cargo door.");
            return;
        }
        pending.pose = RelativeToAircraft(aircraft, connection->rightMeters,
                                          connection->forwardMeters, false);
        pending.pose.headingDegrees = NormalizeDegrees(
            aircraft.headingDegrees + connection->relativeHeadingDegrees + 180.0);
        pending.cargoDoor = {connection->interactivePointIndex,
                             connection->forwardMeters, connection->rightMeters,
                             (aircraft.altitudeFeet - aircraft.groundAltitudeFeet) *
                                     kFeetToMeters +
                                 connection->verticalMeters,
                             NormalizeDegrees(connection->relativeHeadingDegrees + 180.0)};
        break;
    }
    }
    QueueObject(std::move(pending));
}

void GroundServicesThread::QueueObject(PendingCreate pending)
{
    const std::uint64_t token = m_nextCreateToken++;
    const std::string title = pending.object.title;
    const auto position = ToInitialPosition(pending.pose);
    m_pendingCreates.emplace(token, std::move(pending));
    PublishStatus();
    m_simConnect.CreateObject(
        title, position, [this, token](DWORD objectId) {
            if (m_stopping.load()) {
                if (objectId != 0) m_simConnect.RemoveObject(objectId);
                return;
            }
            Post([this, token, objectId] { CompleteCreate(token, objectId); });
        });
}

void GroundServicesThread::QueueAttachments(
    const AircraftSnapshot &aircraft, AircraftId parentObjectId,
    const SpawnPose &parentPose,
    const std::vector<GroundServiceObject> &attachments)
{
    for (const GroundServiceObject &attachment : attachments) {
        PendingCreate child{};
        child.aircraft = aircraft;
        child.object = attachment;
        child.pose = RelativeToParent(parentPose, attachment);
        child.parentObjectId = parentObjectId;
        QueueObject(std::move(child));
    }
}

void GroundServicesThread::CompleteCreate(std::uint64_t token, AircraftId objectId)
{
    const auto pending = m_pendingCreates.find(token);
    if (pending == m_pendingCreates.end()) {
        if (objectId != 0) m_simConnect.RemoveObject(objectId);
        return;
    }
    const PendingCreate created = pending->second;
    m_pendingCreates.erase(pending);
    if (objectId == 0) {
        PublishStatus();
        return;
    }
    if (created.cancelled ||
        (created.parentObjectId != 0 && !m_createdObjects.contains(created.parentObjectId))) {
        m_simConnect.RemoveObject(objectId);
        PublishStatus();
        return;
    }

    m_createdObjects.insert(objectId);
    m_objectsByAircraft[created.aircraft.objectId].insert(objectId);
    m_aircraftByObject[objectId] = created.aircraft.objectId;
    m_createdPoses[objectId] = created.pose;
    if (created.parentObjectId != 0) {
        m_parentByObject[objectId] = created.parentObjectId;
        m_childrenByObject[created.parentObjectId].insert(objectId);
    }

    if (created.cargoDoor) {
        const auto point = created.cargoDoor->interactivePointIndex;
        m_openCargoDoorIndices[created.aircraft.objectId].insert(point);
        m_cargoDoorPointByObject[objectId] = point;
        m_simConnect.SetCargoDoorOpen(created.aircraft.objectId, point, true);
        m_simConnect.FreezeObject(objectId);
        PendingCargoDoorAlignment alignment{};
        alignment.aircraft = created.aircraft;
        alignment.object = created.object;
        alignment.initialPose = created.pose;
        alignment.route = created.route;
        alignment.parentObjectId = created.parentObjectId;
        alignment.cargoDoor = *created.cargoDoor;
        alignment.geometryRequestDue = std::chrono::steady_clock::now() + 250ms;
        m_pendingCargoDoorAlignments.emplace(objectId, std::move(alignment));
        m_simConnect.SetBaggageLoaderRampTarget(objectId, 0.0);
    } else {
        FinalizeCreatedObject(objectId, created, created.pose);
    }
    PublishStatus();
    m_log("Created " + created.object.title + " for aircraft " +
          std::to_string(created.aircraft.objectId) + " as ObjectID " +
          std::to_string(objectId) + ".");
}

void GroundServicesThread::FinalizeCreatedObject(
    AircraftId objectId, const PendingCreate &created, const SpawnPose &actualPose)
{
    if (!m_createdObjects.contains(objectId)) return;
    m_createdPoses[objectId] = actualPose;
    if (created.object.animation) {
        m_animation.AddObject(objectId, *created.object.animation, created.route);
    }
    QueueAttachments(created.aircraft, objectId, actualPose, created.object.attachments);
}

void GroundServicesThread::RemoveForAircraft(AircraftId aircraftId, bool closeCargoDoors)
{
    m_configuredAircraft.erase(aircraftId);
    if (closeCargoDoors) CloseCargoDoors(aircraftId);
    for (auto &[token, pending] : m_pendingCreates) {
        static_cast<void>(token);
        if (pending.aircraft.objectId == aircraftId) pending.cancelled = true;
    }
    std::erase_if(m_pendingCargoDoorAlignments,
                  [aircraftId](const auto &entry) {
                      return entry.second.aircraft.objectId == aircraftId;
                  });
    const auto group = m_objectsByAircraft.find(aircraftId);
    if (group == m_objectsByAircraft.end()) {
        PublishStatus();
        return;
    }
    std::vector<AircraftId> objects(group->second.begin(), group->second.end());
    for (const AircraftId objectId : objects) {
        if (m_createdObjects.contains(objectId)) RemoveCreatedObject(objectId, true);
    }
    PublishStatus();
    if (!objects.empty()) {
        m_log("Requested removal of " + std::to_string(objects.size()) +
              " services for aircraft " + std::to_string(aircraftId) + ".");
    }
}

void GroundServicesThread::RemoveCreatedObject(AircraftId objectId,
                                                bool requestSimulatorRemoval)
{
    const auto children = m_childrenByObject.find(objectId);
    if (children != m_childrenByObject.end()) {
        const std::vector<AircraftId> childIds(children->second.begin(), children->second.end());
        m_childrenByObject.erase(children);
        for (const AircraftId childId : childIds) {
            RemoveCreatedObject(childId, requestSimulatorRemoval);
        }
    }
    m_pendingCargoDoorAlignments.erase(objectId);
    m_animation.RemoveObject(objectId);
    if (requestSimulatorRemoval) m_simConnect.RemoveObject(objectId);

    const auto parent = m_parentByObject.find(objectId);
    if (parent != m_parentByObject.end()) {
        if (const auto siblings = m_childrenByObject.find(parent->second);
            siblings != m_childrenByObject.end()) {
            siblings->second.erase(objectId);
            if (siblings->second.empty()) m_childrenByObject.erase(siblings);
        }
        m_parentByObject.erase(parent);
    }
    const auto owner = m_aircraftByObject.find(objectId);
    if (owner != m_aircraftByObject.end()) {
        if (const auto cargoDoor = m_cargoDoorPointByObject.find(objectId);
            cargoDoor != m_cargoDoorPointByObject.end()) {
            m_simConnect.SetCargoDoorOpen(owner->second, cargoDoor->second, false);
            if (const auto doors = m_openCargoDoorIndices.find(owner->second);
                doors != m_openCargoDoorIndices.end()) {
                doors->second.erase(cargoDoor->second);
                if (doors->second.empty()) m_openCargoDoorIndices.erase(doors);
            }
            m_cargoDoorPointByObject.erase(cargoDoor);
        }
        if (const auto group = m_objectsByAircraft.find(owner->second);
            group != m_objectsByAircraft.end()) {
            group->second.erase(objectId);
            if (group->second.empty()) m_objectsByAircraft.erase(group);
        }
        m_aircraftByObject.erase(owner);
    }
    m_createdPoses.erase(objectId);
    m_cargoDoorPointByObject.erase(objectId);
    m_createdObjects.erase(objectId);
}

void GroundServicesThread::CloseCargoDoors(AircraftId aircraftId)
{
    const auto doors = m_openCargoDoorIndices.find(aircraftId);
    if (doors == m_openCargoDoorIndices.end()) return;
    for (const std::uint32_t pointIndex : doors->second) {
        m_simConnect.SetCargoDoorOpen(aircraftId, pointIndex, false);
    }
    m_openCargoDoorIndices.erase(doors);
}

void GroundServicesThread::HandleObjectRemoved(AircraftId objectId)
{
    const bool isTrackedAircraft = m_configuredAircraft.contains(objectId) ||
        m_objectsByAircraft.contains(objectId) ||
        std::ranges::any_of(m_pendingCreates, [objectId](const auto &entry) {
            return entry.second.aircraft.objectId == objectId;
        }) ||
        std::ranges::any_of(m_pendingCargoDoorAlignments,
                            [objectId](const auto &entry) {
                                return entry.second.aircraft.objectId == objectId;
                            });
    if (isTrackedAircraft) {
        m_log("MSFS removed tracked aircraft ObjectID " + std::to_string(objectId) +
              "; removing its pending and created ground services.");
        m_openCargoDoorIndices.erase(objectId);
        RemoveForAircraft(objectId, false);
        return;
    }

    for (auto &[token, pending] : m_pendingCreates) {
        static_cast<void>(token);
        if (pending.parentObjectId == objectId) pending.cancelled = true;
    }
    const bool wasCreated = m_createdObjects.contains(objectId);
    const auto owner = m_aircraftByObject.find(objectId);
    const AircraftId ownerId = owner == m_aircraftByObject.end() ? 0 : owner->second;
    if (wasCreated) RemoveCreatedObject(objectId, false);
    if (wasCreated) {
        m_log("MSFS removed created ground ObjectID " + std::to_string(objectId) +
              (ownerId != 0 ? " owned by aircraft " + std::to_string(ownerId) : "") +
              "; removed it and its dependent objects from tracking.");
    }
    PublishStatus();
}

void GroundServicesThread::HandleConnection(bool connected)
{
    m_connected = connected;
    if (connected) return;
    m_pendingCreates.clear();
    m_pendingCargoDoorAlignments.clear();
    m_createdObjects.clear();
    m_configuredAircraft.clear();
    m_objectsByAircraft.clear();
    m_aircraftByObject.clear();
    m_parentByObject.clear();
    m_childrenByObject.clear();
    m_createdPoses.clear();
    m_openCargoDoorIndices.clear();
    m_cargoDoorPointByObject.clear();
    m_animation.Reset();
    m_configuration.ClearResolution();
    PublishStatus();
}

void GroundServicesThread::RemoveAllServicesInternal()
{
    for (auto &[token, pending] : m_pendingCreates) {
        static_cast<void>(token);
        pending.cancelled = true;
    }
    m_pendingCargoDoorAlignments.clear();
    std::vector<AircraftId> aircraftIds;
    aircraftIds.reserve(m_openCargoDoorIndices.size());
    for (const auto &[aircraftId, doors] : m_openCargoDoorIndices) {
        static_cast<void>(doors);
        aircraftIds.push_back(aircraftId);
    }
    for (const AircraftId aircraftId : aircraftIds) CloseCargoDoors(aircraftId);

    const std::vector<AircraftId> objects(m_createdObjects.begin(), m_createdObjects.end());
    for (const AircraftId objectId : objects) {
        if (m_createdObjects.contains(objectId)) RemoveCreatedObject(objectId, true);
    }
    m_configuredAircraft.clear();
    m_objectsByAircraft.clear();
    m_aircraftByObject.clear();
    m_parentByObject.clear();
    m_childrenByObject.clear();
    m_createdPoses.clear();
    m_cargoDoorPointByObject.clear();
    PublishStatus();
}

void GroundServicesThread::PublishStatus()
{
    std::scoped_lock lock(m_statusMutex);
    m_status = {m_createdObjects.size(), m_pendingCreates.size(),
                m_configuredAircraft.size()};
}

bool GroundServicesThread::HasPendingFor(AircraftId aircraftId) const
{
    return std::ranges::any_of(m_pendingCreates, [aircraftId](const auto &entry) {
        return entry.second.aircraft.objectId == aircraftId && !entry.second.cancelled;
    }) || std::ranges::any_of(m_pendingCargoDoorAlignments,
                              [aircraftId](const auto &entry) {
                                  return entry.second.aircraft.objectId == aircraftId;
                              });
}

GroundServicesThread::SpawnPose GroundServicesThread::RelativeToAircraft(
    const AircraftSnapshot &aircraft, double relX, double relY, bool faceAircraft)
{
    const auto position = RelativePosition(aircraft.headingDegrees, aircraft.longitude,
                                           aircraft.latitude, aircraft.groundAltitudeFeet,
                                           relY, relX);
    SpawnPose result{position.Latitude, position.Longitude, position.Altitude,
                     position.Heading, true};
    if (faceAircraft) {
        result.headingDegrees = HeadingTowardRelativeOrigin(aircraft.headingDegrees,
                                                            relY, relX);
    }
    return result;
}

GroundServicesThread::SpawnPose GroundServicesThread::RelativeToParent(
    const SpawnPose &parent, const GroundServiceObject &child)
{
    // XYZH deliberately uses a different compact convention from Locations:
    // X is forward and -Y is right. Thus a parent at 090 with [0,-5,0,H]
    // places the child five metres south, at an absolute direction of 180.
    const auto position = RelativePosition(parent.headingDegrees, parent.longitude,
                                           parent.latitude, parent.altitudeFeet,
                                           child.parentX, -child.parentY);
    return {position.Latitude, position.Longitude,
            parent.altitudeFeet + child.parentZ / kFeetToMeters,
            NormalizeDegrees(parent.headingDegrees + child.parentHeadingDegrees),
            parent.onGround && std::abs(child.parentZ) < 0.001};
}

SIMCONNECT_DATA_INITPOSITION GroundServicesThread::ToInitialPosition(const SpawnPose &pose)
{
    SIMCONNECT_DATA_INITPOSITION result{};
    result.Latitude = pose.latitude;
    result.Longitude = pose.longitude;
    result.Altitude = pose.altitudeFeet;
    result.Heading = pose.headingDegrees;
    result.OnGround = pose.onGround ? 1 : 0;
    return result;
}
} // namespace parking_services
