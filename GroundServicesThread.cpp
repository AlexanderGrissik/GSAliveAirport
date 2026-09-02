#include "GroundServicesThread.h"

#include "AircraftTrackerThread.h"
#include "GSObject.h"
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
} // namespace

GroundServicesThread::GroundServicesThread(SimConnectThread &simConnect,
                                           AircraftTrackerThread &aircraftTracker,
                                           AnimationThread &animation,
                                           GroundServicesConfig &configuration, LogSink log)
    : m_simConnect(simConnect), m_aircraftTracker(aircraftTracker),
      m_animation(animation), m_configuration(configuration), m_log(std::move(log))
{
    m_services = MakeServices();
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
        if (m_connected) MaintainObjects(now);
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

// Drives one maintenance tick for every live object. Each object decides for
// itself whether it needs anything (e.g. a cargo-door loader requesting ramp
// geometry); objects that finish (cancelled, or created with no object to track)
// are retired and dropped from the collection.
void GroundServicesThread::MaintainObjects(std::chrono::steady_clock::time_point now)
{
    std::vector<std::uint64_t> retired;
    for (auto &[token, object] : m_objects) {
        if (object->Cancelled() || object->Finalized()) continue;
        object->Maintain(m_services, now);
        if (object->Retired()) retired.push_back(token);
    }
    for (const std::uint64_t token : retired) m_objects.erase(token);
    if (!retired.empty()) PublishStatus();
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
    for (const auto &[token, object] : m_objects) {
        static_cast<void>(token);
        owned.insert(object->AircraftObjectId());
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
          " (wingspan " + std::to_string(aircraft.wingSpanMeters) + " m.");
    PublishStatus();
}

// Builds the GSObject for one request (dispatching on specialType inside the
// factory), resolves its placement, and starts the create. The concrete
// subclass encapsulates all type-specific placement logic (route walking,
// cargo-door attachment, ...).
void GroundServicesThread::QueueService(const AircraftSnapshot &aircraft,
                                        const GroundServiceRequest &request)
{
    const std::uint64_t token = m_nextCreateToken++;
    auto object = GSObject::Create(token, aircraft, request);
    if (!object) return;
    if (!object->PreparePlacement(m_services)) {
        m_log("Skipped " + request.object.family + " for aircraft " +
              std::to_string(aircraft.objectId) + ": no valid placement was found.");
        return;
    }
    BeginCreate(token, std::move(object));
}
// Starts the async create for one object. The object itself (a GSObject
// subclass) already knows its pose and route; the callback hands the result
// back to the object through OnCreated, which decides whether to finalize or
// continue (e.g. a cargo-door loader defers until its ramp aligns).
void GroundServicesThread::BeginCreate(std::uint64_t token, std::unique_ptr<GSObject> object)
{
    auto *raw = object.get();
    const GSObjectSpawnPose pose = raw->Pose();
    m_objects.emplace(token, std::move(object));
    PublishStatus();
    m_simConnect.CreateObject(raw->Object().title, GSObject::ToInitialPosition(pose),
                              [this, token](DWORD objectId) {
                                  if (m_stopping.load()) {
                                      if (objectId != 0) m_simConnect.RemoveObject(objectId);
                                      return;
                                  }
                                  Post([this, token, objectId] {
                                      const auto it = m_objects.find(token);
                                      if (it == m_objects.end()) {
                                          if (objectId != 0) m_simConnect.RemoveObject(objectId);
                                          return;
                                      }
                                      it->second->OnCreated(m_services, objectId);
                                  });
                              });
}

// Records a successfully created object (bookkeeping). Called by GSObject via
// the services.registerObject capability, before type-specific activation.
void GroundServicesThread::RegisterObject(GSObject *object, AircraftId objectId,
                                          const GSObjectSpawnPose &pose)
{
    static_cast<void>(pose);
    m_createdObjects.insert(objectId);
    m_objectsByAircraft[object->AircraftObjectId()].insert(objectId);
    m_aircraftByObject[objectId] = object->AircraftObjectId();
    if (object->ParentObjectId() != 0) {
        m_parentByObject[objectId] = object->ParentObjectId();
        m_childrenByObject[object->ParentObjectId()].insert(objectId);
    }
}

void GroundServicesThread::QueueAttachments(const AircraftSnapshot &aircraft,
                                            AircraftId parentObjectId,
                                            const GSObjectSpawnPose &parentPose,
                                            const std::vector<GroundServiceObject> &attachments)
{
    for (const GroundServiceObject &attachment : attachments) {
        const std::uint64_t token = m_nextCreateToken++;
        auto object = std::make_unique<GSObject>(token, aircraft, attachment,
                                                 GroundServiceLocation{}, parentObjectId);
        object->PrepareAttachment(parentPose);
        BeginCreate(token, std::move(object));
    }
}

// Shared completion (GSObject::Finish): start the walking animation (with the
// route, if any) and queue any child attachments. Called via services.finalize.
void GroundServicesThread::FinalizeObject(GSObject *object, const GSObjectSpawnPose &actualPose)
{
    if (object->Object().animation) {
        m_animation.AddObject(object->ObjectId(), *object->Object().animation, object->Route());
    }
    QueueAttachments(object->Aircraft(), object->ObjectId(), actualPose,
                     object->Object().attachments);
}
void GroundServicesThread::RemoveForAircraft(AircraftId aircraftId)
{
    m_configuredAircraft.erase(aircraftId);
    std::vector<std::uint64_t> tokens;
    for (auto &[token, object] : m_objects) {
        if (object->AircraftObjectId() == aircraftId) tokens.push_back(token);
    }
    for (const std::uint64_t token : tokens) {
        if (auto it = m_objects.find(token); it != m_objects.end()) {
            RemoveObject(it->second->ObjectId(), true);
        }
    }
    PublishStatus();
    if (!tokens.empty()) {
        m_log("Requested removal of " + std::to_string(tokens.size()) +
              " services for aircraft " + std::to_string(aircraftId) + ".");
    }
}

// Removes one created object (and any children first). The object's OnRemoved
// hook runs here, letting it close its own cargo door (luggage loaders); other
// types leave it a no-op.
void GroundServicesThread::RemoveObject(AircraftId objectId, bool requestSimulatorRemoval)
{
    if (objectId == 0) return;

    std::vector<AircraftId> children;
    if (const auto childIt = m_childrenByObject.find(objectId);
        childIt != m_childrenByObject.end()) {
        children.assign(childIt->second.begin(), childIt->second.end());
    }
    for (const AircraftId childId : children) RemoveObject(childId, requestSimulatorRemoval);

    GSObject *object = nullptr;
    std::uint64_t token = 0;
    for (auto &entry : m_objects) {
        if (entry.second->ObjectId() == objectId) {
            object = entry.second.get();
            token = entry.first;
            break;
        }
    }
    if (object) {
        object->OnRemoved(m_services);
        m_objects.erase(token);
    }

    m_animation.RemoveObject(objectId);
    if (requestSimulatorRemoval) m_simConnect.RemoveObject(objectId);

    m_childrenByObject.erase(objectId);
    if (const auto parent = m_parentByObject.find(objectId); parent != m_parentByObject.end()) {
        if (const auto siblings = m_childrenByObject.find(parent->second);
            siblings != m_childrenByObject.end()) {
            siblings->second.erase(objectId);
            if (siblings->second.empty()) m_childrenByObject.erase(siblings);
        }
        m_parentByObject.erase(parent);
    }
    if (const auto owner = m_aircraftByObject.find(objectId); owner != m_aircraftByObject.end()) {
        if (const auto group = m_objectsByAircraft.find(owner->second);
            group != m_objectsByAircraft.end()) {
            group->second.erase(objectId);
            if (group->second.empty()) m_objectsByAircraft.erase(group);
        }
        m_aircraftByObject.erase(owner);
    }
    m_createdObjects.erase(objectId);
}

void GroundServicesThread::HandleObjectRemoved(AircraftId objectId)
{
    const bool isTrackedAircraft =
        m_configuredAircraft.contains(objectId) || m_objectsByAircraft.contains(objectId) ||
        std::ranges::any_of(m_objects, [objectId](const auto &entry) {
            return entry.second->AircraftObjectId() == objectId;
        });
    if (isTrackedAircraft) {
        m_log("MSFS removed tracked aircraft ObjectID " + std::to_string(objectId) +
              "; removing its pending and created ground services.");
        RemoveForAircraft(objectId);
        return;
    }

    // A created ground object (or one of its parents) was removed by MSFS.
    // Cancel any pending children that depended on it, then remove it (and
    // remove dependents that were already created).
    std::vector<std::uint64_t> tokens;
    for (auto &[token, object] : m_objects) {
        if (object->ParentObjectId() == objectId) tokens.push_back(token);
    }
    for (const std::uint64_t token : tokens) {
        if (auto it = m_objects.find(token); it != m_objects.end()) {
            if (it->second->Finalized()) {
                RemoveObject(it->second->ObjectId(), true);
            } else {
                it->second->Cancel();
            }
        }
    }
    const bool wasCreated = m_createdObjects.contains(objectId);
    if (wasCreated) RemoveObject(objectId, false);
    if (wasCreated) {
        m_log("MSFS removed created ground ObjectID " + std::to_string(objectId) +
              "; removed it and its dependent objects from tracking.");
    }
    PublishStatus();
}
void GroundServicesThread::HandleConnection(bool connected)
{
    m_connected = connected;
    if (connected) return;
    RemoveAllServicesInternal();
    m_animation.Reset();
    m_configuration.ClearResolution();
    PublishStatus();
}

void GroundServicesThread::RemoveAllServicesInternal()
{
    std::vector<AircraftId> objects;
    objects.assign(m_createdObjects.begin(), m_createdObjects.end());
    for (const AircraftId objectId : objects) {
        if (m_createdObjects.contains(objectId)) RemoveObject(objectId, true);
    }
    m_objects.clear();
    m_configuredAircraft.clear();
    m_objectsByAircraft.clear();
    m_aircraftByObject.clear();
    m_parentByObject.clear();
    m_childrenByObject.clear();
    PublishStatus();
}

void GroundServicesThread::PublishStatus()
{
    std::size_t pendingCreates = 0;
    for (const auto &[token, object] : m_objects) {
        static_cast<void>(token);
        if (!object->Retired()) pendingCreates++;
    }
    std::scoped_lock lock(m_statusMutex);
    m_status = {m_createdObjects.size(), pendingCreates, m_configuredAircraft.size()};
}

bool GroundServicesThread::HasPendingFor(AircraftId aircraftId) const
{
    return std::ranges::any_of(m_objects, [aircraftId](const auto &entry) {
        return entry.second->AircraftObjectId() == aircraftId && !entry.second->Retired();
    });
}
// The capabilities a GSObject calls into. These are the only SimConnect /
// animation touchpoints the driver exposes to the objects, which keeps them
// decoupled from those classes. Everything here runs on the ground-services
// thread, so it may safely mutate shared state.
GSObjectServices GroundServicesThread::MakeServices()
{
    GSObjectServices services;
    services.log = [this](std::string message) { m_log(std::move(message)); };
    services.registerObject = [this](GSObject *object, AircraftId objectId,
                                     const GSObjectSpawnPose &pose) {
        if (object) RegisterObject(object, objectId, pose);
    };
    services.finalize = [this](GSObject *object, const GSObjectSpawnPose &pose) {
        if (object) FinalizeObject(object, pose);
    };
    services.removeSimObject = [this](AircraftId objectId) { m_simConnect.RemoveObject(objectId); };
    services.setPosition = [this](AircraftId objectId, const GSObjectSpawnPose &pose) {
        const ObjectPositionUpdate update{objectId, pose.latitude, pose.longitude,
                                          pose.altitudeFeet, pose.headingDegrees};
        m_simConnect.PublishObjectPositionUpdates({update});
    };
    services.freezeObject = [this](AircraftId objectId) { m_simConnect.FreezeObject(objectId); };
    services.openCargoDoor = [this](AircraftId aircraftId, std::uint32_t point) {
        m_simConnect.SetCargoDoorOpen(aircraftId, point, true);
    };
    services.closeCargoDoor = [this](AircraftId aircraftId, std::uint32_t point) {
        m_simConnect.SetCargoDoorOpen(aircraftId, point, false);
    };
    services.setRampTarget = [this](AircraftId objectId, double angle) {
        m_simConnect.SetBaggageLoaderRampTarget(objectId, angle);
    };
    services.requestBaggageGeometry = [this](AircraftId objectId) {
        m_simConnect.RequestBaggageLoaderGeometry(
            objectId, [this, objectId](BaggageLoaderGeometry geometry) {
                Post([this, objectId, geometry] {
                    for (auto &entry : m_objects) {
                        GSObject *object = entry.second.get();
                        if (object->ObjectId() == objectId) {
                            object->OnGeometry(m_services, geometry);
                            return;
                        }
                    }
                });
            });
    };
    services.isParentCreated = [this](AircraftId parentObjectId) {
        return m_createdObjects.contains(parentObjectId);
    };
    return services;
}
} // namespace parking_services