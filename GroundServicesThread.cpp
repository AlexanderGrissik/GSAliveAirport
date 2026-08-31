#include "GroundServicesThread.h"

#include "AircraftTrackerThread.h"
#include "AnimationThread.h"
#include "SimConnectIds.h"
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
constexpr std::string_view kFsdtCateringTitle = "FSDT_Catering_EU";
constexpr std::string_view kBaggageCartTitle = "ASO_Baggage_Cart01";
constexpr std::string_view kFsdtWorkerTitle = "FSDT_catering_man_01";
constexpr std::string_view kFsdtWingwalkerTitle = "FSDT_Wingwalker_Male_04";
constexpr std::string_view kFsdtMarshallerTitle = "FSDT_Marshaller_01";
constexpr std::string_view kAsoboMarshallerTitle = "Marshaller_Male_Summer_Caucasian";

bool SafeParkedAircraft(const AircraftSnapshot &aircraft)
{
    return !aircraft.isUser && aircraft.onGround &&
           std::abs(aircraft.groundSpeedKnots) < kRemovalSpeedKnots;
}
}

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

void GroundServicesThread::SpawnFullTest()
{
    Post([this] { SpawnFullTestInternal(); });
}

void GroundServicesThread::ClearCreated(bool announce)
{
    Post([this, announce] { ClearCreatedInternal(announce); });
}

void GroundServicesThread::Reset()
{
    Post([this] {
        ClearCreatedInternal(false);
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
        if (m_connected && now >= nextDecision) {
            EvaluateTrackedAircraft();
            nextDecision = now + 10s;
        }
        std::unique_lock lock(m_commandMutex);
        m_wake.wait_for(lock, stopToken, 100ms, [this] { return !m_commands.empty(); });
    }
    ProcessCommands();
    ClearCreatedInternal(false);
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
    for (const auto &[aircraftId, objects] : m_objectsByAircraft) owned.insert(aircraftId);
    for (const auto &[token, pending] : m_pendingCreates) owned.insert(pending.aircraft.objectId);
    for (const AircraftId aircraftId : owned) {
        if (!present.contains(aircraftId)) RemoveForAircraft(aircraftId);
    }
}

void GroundServicesThread::ResolveConfigurationIfAvailable()
{
    if (!m_configuration.IsLoaded() || m_configuration.IsResolved() ||
        !m_simConnect.FillAvailableSimObjectTitles(m_catalogTitleBuffer)) return;
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
        std::optional<RelativeWalkingPath> walkingPath;
        if (request.walking) {
            walkingPath = RelativeWalkingPath{request.relX1, request.relY1,
                                              request.relX2, request.relY2};
        }
        RequestObject(aircraft, request.title, request.relY1, request.relX1,
                      walkingPath, request.faceAircraft);
    }
    m_log("Applied " + std::to_string(m_serviceRequestBuffer.size()) +
          " configured " + std::string(AircraftSizeCategoryName(*category)) +
          " service request(s) to aircraft " + std::to_string(aircraft.objectId) +
          " (wingspan " + std::to_string(aircraft.wingSpanMeters) + " m).");
    PublishStatus();
}

void GroundServicesThread::RemoveForAircraft(AircraftId aircraftId)
{
    m_configuredAircraft.erase(aircraftId);
    for (auto &[token, pending] : m_pendingCreates) {
        if (pending.aircraft.objectId == aircraftId) pending.cancelled = true;
    }
    const auto group = m_objectsByAircraft.find(aircraftId);
    if (group == m_objectsByAircraft.end()) {
        PublishStatus();
        return;
    }
    const auto objects = group->second;
    m_objectsByAircraft.erase(group);
    for (const AircraftId objectId : objects) {
        m_animation.RemoveWorker(objectId);
        m_simConnect.RemoveObject(objectId);
        m_createdObjects.erase(objectId);
        m_aircraftByObject.erase(objectId);
    }
    PublishStatus();
    if (!objects.empty()) {
        m_log("Requested removal of " + std::to_string(objects.size()) +
              " services for aircraft " + std::to_string(aircraftId) + ".");
    }
}

void GroundServicesThread::RequestObject(const AircraftSnapshot &aircraft,
                                         std::string title, double forwardMeters,
                                         double rightMeters,
                                         std::optional<RelativeWalkingPath> walkingPath,
                                         bool faceAircraft)
{
    const std::uint64_t token = m_nextCreateToken++;
    m_pendingCreates.emplace(
        token, PendingCreate{aircraft, title, std::move(walkingPath), false});
    PublishStatus();
    auto position = RelativePosition(aircraft.headingDegrees, aircraft.longitude,
                                     aircraft.latitude, aircraft.altitudeFeet,
                                     forwardMeters, rightMeters);
    if (faceAircraft) {
        position.Heading = HeadingTowardRelativeOrigin(
            aircraft.headingDegrees, forwardMeters, rightMeters);
    }
    m_simConnect.CreateObject(
        title, position,
        [this, token](DWORD objectId) {
            if (m_stopping.load()) {
                if (objectId != 0) m_simConnect.RemoveObject(objectId);
                return;
            }
            Post([this, token, objectId] { CompleteCreate(token, objectId); });
        });
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
    if (created.cancelled) {
        m_simConnect.RemoveObject(objectId);
        PublishStatus();
        m_log("Immediately removed late-created " + created.title +
              " for inactive aircraft " + std::to_string(created.aircraft.objectId) + ".");
        return;
    }
    m_createdObjects.insert(objectId);
    m_objectsByAircraft[created.aircraft.objectId].insert(objectId);
    m_aircraftByObject[objectId] = created.aircraft.objectId;
    if (created.walkingPath) {
        m_animation.AddWorker(objectId, created.title, created.aircraft,
                              *created.walkingPath);
    } else if (created.title == kFsdtWingwalkerTitle ||
               created.title == kFsdtMarshallerTitle) {
        m_animation.AddWorker(objectId, created.title, created.aircraft);
    }
    PublishStatus();
    m_log("Created " + created.title + " for aircraft " +
          std::to_string(created.aircraft.objectId) + " as ObjectID " +
          std::to_string(objectId) + ".");
}

void GroundServicesThread::HandleObjectRemoved(AircraftId objectId)
{
    const bool isParent = m_configuredAircraft.contains(objectId) ||
        m_objectsByAircraft.contains(objectId) ||
        std::ranges::any_of(m_pendingCreates, [objectId](const auto &entry) {
            return entry.second.aircraft.objectId == objectId;
        });
    if (isParent) {
        m_log("MSFS removed tracked aircraft ObjectID " + std::to_string(objectId) +
              "; removing its pending and created ground services.");
        RemoveForAircraft(objectId);
        return;
    }
    const bool wasCreated = m_createdObjects.contains(objectId);
    m_animation.RemoveWorker(objectId);
    m_createdObjects.erase(objectId);
    const auto owner = m_aircraftByObject.find(objectId);
    AircraftId ownerId = 0;
    if (owner != m_aircraftByObject.end()) {
        ownerId = owner->second;
        if (auto group = m_objectsByAircraft.find(owner->second);
            group != m_objectsByAircraft.end()) {
            group->second.erase(objectId);
            if (group->second.empty()) m_objectsByAircraft.erase(group);
        }
        m_aircraftByObject.erase(owner);
    }
    PublishStatus();
    if (wasCreated) {
        m_log("MSFS removed created ground ObjectID " + std::to_string(objectId) +
              (ownerId != 0 ? " owned by aircraft " + std::to_string(ownerId) : "") +
              "; removed it from service and animation tracking.");
    }
}

void GroundServicesThread::HandleConnection(bool connected)
{
    m_connected = connected;
    if (connected) return;
    m_pendingCreates.clear();
    m_createdObjects.clear();
    m_configuredAircraft.clear();
    m_objectsByAircraft.clear();
    m_aircraftByObject.clear();
    m_animation.Reset();
    m_configuration.ClearResolution();
    PublishStatus();
}

void GroundServicesThread::SpawnFullTestInternal()
{
    if (!m_connected) {
        m_log("Cannot create test objects: not connected to MSFS.");
        return;
    }
    std::size_t aircraftCount = 0;
    std::size_t requested = 0;
    m_aircraftTracker.FillNearbyAircraftSnapshot(m_aircraftSnapshotBuffer);
    for (const auto &aircraft : m_aircraftSnapshotBuffer) {
        if (!SafeParkedAircraft(aircraft)) continue;
        ++aircraftCount;
        RequestObject(aircraft, std::string(kFsdtCateringTitle), 0.0, 22.0);
        RequestObject(aircraft, std::string(kBaggageCartTitle), -8.0, 28.0);
        RequestObject(aircraft, std::string(kFsdtWorkerTitle), 3.0, 19.0);
        RequestObject(aircraft, std::string(kFsdtWingwalkerTitle), -3.0, 19.0);
        RequestObject(aircraft, std::string(kFsdtMarshallerTitle), 0.0, 14.0);
        RequestObject(aircraft, std::string(kAsoboMarshallerTitle), 4.5, 14.0);
        requested += 6;
    }
    m_log("Requested " + std::to_string(requested) + " test objects for " +
          std::to_string(aircraftCount) + " parked aircraft.");
}

void GroundServicesThread::ClearCreatedInternal(bool announce)
{
    for (auto &[token, pending] : m_pendingCreates) pending.cancelled = true;
    const auto objectCount = m_createdObjects.size();
    for (const AircraftId objectId : m_createdObjects) {
        m_animation.RemoveWorker(objectId);
        m_simConnect.RemoveObject(objectId);
    }
    m_createdObjects.clear();
    m_configuredAircraft.clear();
    m_objectsByAircraft.clear();
    m_aircraftByObject.clear();
    PublishStatus();
    if (announce) {
        m_log("Requested removal of " + std::to_string(objectCount) +
              " created service objects.");
    }
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
    });
}
} // namespace parking_services
