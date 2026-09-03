#include "GroundServicesThread.h"

#include "AircraftTrackerThread.h"
#include "GSObject.h"
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
constexpr SIMCONNECT_DATA_DEFINITION_ID kRampTargetDefinition = 4;
constexpr SIMCONNECT_DATA_DEFINITION_ID kGeometryDefinition = 5;
constexpr SIMCONNECT_DATA_DEFINITION_ID kPositionDefinition = 7;
constexpr SIMCONNECT_CLIENT_EVENT_ID kFreezeLatitudeLongitudeEvent = 20;
constexpr SIMCONNECT_CLIENT_EVENT_ID kFreezeAltitudeEvent = 21;
constexpr SIMCONNECT_CLIENT_EVENT_ID kFreezeAttitudeEvent = 22;
constexpr SIMCONNECT_CLIENT_EVENT_ID kOpenAircraftDoorsEvent = 23;
constexpr SIMCONNECT_CLIENT_EVENT_ID kCloseAircraftDoorsEvent = 24;

#pragma pack(push, 1)
struct BaggageLoaderRampTargetWireData
{
    double angleDegrees{};
};
#pragma pack(pop)

static_assert(sizeof(BaggageLoaderRampTargetWireData) == 8);
} // namespace

GroundServicesThread::GroundServicesThread(ISimConnectHandler &simConnect,
                                           AircraftTrackerThread &aircraftTracker,
                                           AnimationThread &animation,
                                           GroundServicesConfig configuration, LogSink log)
    : m_simConnect(simConnect), m_aircraftTracker(aircraftTracker),
      m_animation(animation), m_configuration(std::move(configuration)), m_log(std::move(log))
{
    for (const std::string &message : m_configuration.StartupMessages()) {
        m_log(message);
    }
    m_services = MakeServices();
}

GroundServicesThread::~GroundServicesThread()
{
    Stop();
}

void GroundServicesThread::Start()
{
    if (m_thread.joinable()) return;
    m_thread = std::jthread(&GroundServicesThread::GroundServicesLoop, this);
}

void GroundServicesThread::Stop()
{
    if (!m_thread.joinable()) return;
    m_thread.request_stop();
    m_wake.notify_all();
    m_thread.join();
}

void GroundServicesThread::OnSimConnected()
{
    Post([this] { InitializeSimConnect(); });
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
        PollRequests();
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

void GroundServicesThread::InitializeSimConnect()
{
    CollectFinishedCommandRequests();
    m_simConnect.AddDatum(kRampTargetDefinition, "BAGGAGELOADER ANGLE TARGET",
                          "degrees", SIMCONNECT_DATATYPE_FLOAT64,
                          NewCommandRequest());
    m_simConnect.AddDatum(kGeometryDefinition, "BAGGAGELOADER ANGLE CURRENT",
                          "degrees", SIMCONNECT_DATATYPE_FLOAT64,
                          NewCommandRequest());
    m_simConnect.AddDatum(kGeometryDefinition, "BAGGAGELOADER END RAMP Y",
                          "meters", SIMCONNECT_DATATYPE_FLOAT64,
                          NewCommandRequest());
    m_simConnect.AddDatum(kGeometryDefinition, "BAGGAGELOADER END RAMP Z",
                          "meters", SIMCONNECT_DATATYPE_FLOAT64,
                          NewCommandRequest());
    m_simConnect.AddDatum(kGeometryDefinition, "BAGGAGELOADER PIVOT Y", "meters",
                          SIMCONNECT_DATATYPE_FLOAT64, NewCommandRequest());
    m_simConnect.AddDatum(kGeometryDefinition, "BAGGAGELOADER PIVOT Z", "meters",
                          SIMCONNECT_DATATYPE_FLOAT64, NewCommandRequest());
    m_simConnect.AddDatum(kPositionDefinition, "Initial Position", "",
                          SIMCONNECT_DATATYPE_INITPOSITION, NewCommandRequest());
    m_simConnect.MapClientEvent(kFreezeLatitudeLongitudeEvent,
                                "FREEZE_LATITUDE_LONGITUDE_SET",
                                NewCommandRequest());
    m_simConnect.MapClientEvent(kFreezeAltitudeEvent, "FREEZE_ALTITUDE_SET",
                                NewCommandRequest());
    m_simConnect.MapClientEvent(kFreezeAttitudeEvent, "FREEZE_ATTITUDE_SET",
                                NewCommandRequest());
    m_simConnect.MapClientEvent(kOpenAircraftDoorsEvent, "OPEN_AIRCRAFT_DOORS",
                                NewCommandRequest());
    m_simConnect.MapClientEvent(kCloseAircraftDoorsEvent, "CLOSE_AIRCRAFT_DOORS",
                                NewCommandRequest());
}

void GroundServicesThread::PollRequests()
{
    CollectFinishedCommandRequests();

    std::vector<std::uint64_t> completedCreates;
    for (const auto &[token, operation] : m_createRequests) {
        if (operation.request->IsFinished()) completedCreates.push_back(token);
    }
    for (const std::uint64_t token : completedCreates) {
        const auto operationIt = m_createRequests.find(token);
        if (operationIt == m_createRequests.end()) continue;
        const AircraftId aircraftId = operationIt->second.aircraftId;
        const DWORD objectId = operationIt->second.request->ObjectId();
        m_createRequests.erase(operationIt);

        const auto object = m_objects.find(token);
        if (object == m_objects.end()) {
            if (objectId != 0) {
                m_simConnect.RemoveObject(objectId, NewCommandRequest());
            }
            DecrementPendingCreations(aircraftId);
            continue;
        }
        object->second->OnCreated(m_services, objectId);
        DecrementPendingCreations(aircraftId);
    }

    std::vector<AircraftId> completedGeometry;
    for (const auto &[objectId, request] : m_geometryRequests) {
        if (request->IsFinished()) completedGeometry.push_back(objectId);
    }
    for (const AircraftId objectId : completedGeometry) {
        const auto requestIt = m_geometryRequests.find(objectId);
        if (requestIt == m_geometryRequests.end()) continue;
        const GSBaggageGeometryResult result = requestIt->second->Result();
        m_geometryRequests.erase(requestIt);

        for (auto &entry : m_objects) {
            GSObject *object = entry.second.get();
            if (object->ObjectId() != objectId) continue;
            object->OnGeometry(
                m_services,
                {result.succeeded, result.angleCurrentDegrees, result.endRampYMeters,
                 result.endRampZMeters, result.pivotYMeters, result.pivotZMeters});
            break;
        }
    }
}

GSReqCommand &GroundServicesThread::NewCommandRequest()
{
    auto request = std::make_unique<GSReqCommand>();
    GSReqCommand &reference = *request;
    m_commandRequests.push_back(std::move(request));
    return reference;
}

void GroundServicesThread::CollectFinishedCommandRequests()
{
    std::erase_if(m_commandRequests, [](const auto &request) {
        return request->IsFinished();
    });
}

// Drives one maintenance tick for every live object. Each object decides for
// itself whether it needs anything (e.g. a cargo-door loader requesting ramp
// geometry); objects that finish (cancelled, or created with no object to track)
// are retired and dropped from the collection.
void GroundServicesThread::MaintainObjects(std::chrono::steady_clock::time_point now)
{
    std::vector<std::uint64_t> retired;
    for (auto &[token, object] : m_objects) {
        if (object->Retired()) {
            retired.push_back(token);
            continue;
        }
        // Still in flight (create not yet resolved) or already finalized: leave it
        // to its create request, or let it keep doing its animation/geometry work.
        if (!object->Resolved() || object->Finalized()) continue;
        object->Maintain(m_services, now);
    }
    for (const std::uint64_t token : retired) m_objects.erase(token);
    if (!retired.empty()) PublishStatus();
}
void GroundServicesThread::EvaluateTrackedAircraft()
{
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
// subclass) already knows its pose and route; the request hands the result back
// to the object through OnCreated, which decides whether to finalize or
// continue (e.g. a cargo-door loader defers until its ramp aligns).
void GroundServicesThread::BeginCreate(std::uint64_t token, std::unique_ptr<GSObject> object)
{
    auto *raw = object.get();
    const GSObject::GSObjectPos pose = raw->Pose();
    const AircraftId aircraftId = raw->AircraftObjectId();
    m_objects.emplace(token, std::move(object));
    m_pendingCreations[aircraftId]++;
    PublishStatus();
    auto request = std::make_unique<GSReqCreateObject>();
    GSReqCreateObject &requestReference = *request;
    m_createRequests.emplace(token, CreateOperation{aircraftId, std::move(request)});
    m_simConnect.CreateObject(raw->Object().title, GSObject::ToInitialPosition(pose),
                              requestReference);
}

// Records a successfully created object (bookkeeping). Called by GSObject via
// the services.registerObject capability, before type-specific activation.
void GroundServicesThread::RegisterObject(GSObject *object, AircraftId objectId,
                                          const GSObject::GSObjectPos &pose)
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
                                            const GSObject::GSObjectPos &parentPose,
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
void GroundServicesThread::FinalizeObject(GSObject *object,
                                          const GSObject::GSObjectPos &actualPose)
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
    const std::size_t pending = PendingCount(aircraftId);
    RemoveResolvedObjects(aircraftId, /*requestSimulatorRemoval=*/true);
    if (pending > 0) {
        // One or more creations are still in flight; let them complete and finish
        // the removal from DecrementPendingCreations once the count drains to zero.
        m_deferredRemoval.insert(aircraftId);
        m_log("Deferring removal of aircraft " + std::to_string(aircraftId) + ": " +
              std::to_string(pending) + " ground-service creation(s) still in flight.");
    }
    PublishStatus();
}

// Removes only this aircraft's objects whose create has already resolved
// (created objects via RemoveObject; failed creates are dropped from m_objects).
// In-flight objects are left untouched so their create can still complete.
void GroundServicesThread::RemoveResolvedObjects(AircraftId aircraftId,
                                                 bool requestSimulatorRemoval)
{
    std::vector<std::uint64_t> tokens;
    for (auto &[token, object] : m_objects) {
        if (object->AircraftObjectId() != aircraftId) continue;
        if (!object->Resolved()) continue;
        tokens.push_back(token);
    }
    for (const std::uint64_t token : tokens) {
        const auto it = m_objects.find(token);
        if (it == m_objects.end()) continue;
        const AircraftId objectId = it->second->ObjectId();
        if (objectId != 0) {
            RemoveObject(objectId, requestSimulatorRemoval);
        } else {
            m_objects.erase(it);
        }
    }
}

// Completes a previously deferred removal: every one of the aircraft's creations
// has now resolved, so remove what remains and clear its bookkeeping.
void GroundServicesThread::FinalizeDeferredRemoval(AircraftId aircraftId)
{
    if (m_deferredRemoval.erase(aircraftId) == 0) return;
    RemoveResolvedObjects(aircraftId, /*requestSimulatorRemoval=*/true);
    m_configuredAircraft.erase(aircraftId);
    m_log("Completed deferred removal of services for aircraft " +
          std::to_string(aircraftId) + ".");
    PublishStatus();
}

std::size_t GroundServicesThread::PendingCount(AircraftId aircraftId) const
{
    const auto it = m_pendingCreations.find(aircraftId);
    return it == m_pendingCreations.end() ? 0 : it->second;
}

// Called (on the GS thread) when a creation for an aircraft resolves. When the
// aircraft's in-flight count drains to zero and a removal was deferred for it,
// finish that removal now.
void GroundServicesThread::DecrementPendingCreations(AircraftId aircraftId)
{
    const auto it = m_pendingCreations.find(aircraftId);
    if (it == m_pendingCreations.end()) return;
    if (--it->second == 0) {
        m_pendingCreations.erase(it);
        if (m_deferredRemoval.contains(aircraftId)) FinalizeDeferredRemoval(aircraftId);
    }
    PublishStatus();
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
    if (requestSimulatorRemoval) {
        m_simConnect.RemoveObject(objectId, NewCommandRequest());
    }

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

    // A created ground object was removed by MSFS. Created children are removed
    // now; still-in-flight children are deferred (their create still completes) and
    // finalized by DecrementPendingCreations once it resolves.
    std::vector<std::uint64_t> tokens;
    for (auto &[token, object] : m_objects) {
        if (object->ParentObjectId() == objectId) tokens.push_back(token);
    }
    for (const std::uint64_t token : tokens) {
        if (auto it = m_objects.find(token); it != m_objects.end()) {
            if (it->second->Resolved()) {
                RemoveObject(it->second->ObjectId(), true);
            } else {
                m_deferredRemoval.insert(it->second->AircraftObjectId());
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
void GroundServicesThread::OnSimStarted()
{
    Post([this] { HandleConnect(); });
}

void GroundServicesThread::OnSimDisconnected()
{
    Post([this] { HandleDisconnect(); });
}

void GroundServicesThread::OnSimStopped()
{
    Post([this] { HandleDisconnect(); });
}

void GroundServicesThread::OnObjRemoved(std::uint32_t objectId)
{
    Post([this, objectId] { HandleObjectRemoved(objectId); });
}

void GroundServicesThread::HandleConnect()
{
    // Load (or refresh) the SimObject catalog on the SimConnect thread and
    // resolve the configured service families against it. Runs on this thread
    // and blocks only until the catalog request completes.
    m_configuration.LoadCatalog(m_simConnect);
    for (const std::string &message : m_configuration.InitializationMessages()) {
        m_log(message);
    }
    m_connected = true;
    PublishStatus();
}

void GroundServicesThread::HandleDisconnect()
{
    m_connected = false;
    RemoveAllServicesInternal();
    m_animation.Reset();
    m_configuration.ResetInitialization();
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
    m_pendingCreations.clear();
    m_deferredRemoval.clear();
    PublishStatus();
}

void GroundServicesThread::PublishStatus()
{
    std::size_t pendingCreates = 0;
    for (const auto &[aircraftId, count] : m_pendingCreations) {
        static_cast<void>(aircraftId);
        pendingCreates += count;
    }
    std::scoped_lock lock(m_statusMutex);
    m_status = {m_createdObjects.size(), pendingCreates, m_configuredAircraft.size()};
}

bool GroundServicesThread::HasPendingFor(AircraftId aircraftId) const
{
    return m_pendingCreations.contains(aircraftId);
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
                                     const GSObject::GSObjectPos &pose) {
        if (object) RegisterObject(object, objectId, pose);
    };
    services.finalize = [this](GSObject *object, const GSObject::GSObjectPos &pose) {
        if (object) FinalizeObject(object, pose);
    };
    services.removeSimObject = [this](AircraftId objectId) {
        m_simConnect.RemoveObject(objectId, NewCommandRequest());
    };
    services.setPosition = [this](AircraftId objectId, const GSObject::GSObjectPos &pose) {
        SIMCONNECT_DATA_INITPOSITION position = GSObject::ToInitialPosition(pose);
        position.OnGround = 1;
        m_simConnect.SetObjectData(kPositionDefinition, objectId, 0,
                                   sizeof(position), &position, NewCommandRequest());
    };
    services.freezeObject = [this](AircraftId objectId) {
        m_simConnect.TransmitEvent(objectId, kFreezeLatitudeLongitudeEvent, 1,
                                   NewCommandRequest());
        m_simConnect.TransmitEvent(objectId, kFreezeAltitudeEvent, 1,
                                   NewCommandRequest());
        m_simConnect.TransmitEvent(objectId, kFreezeAttitudeEvent, 1,
                                   NewCommandRequest());
    };
    services.openCargoDoor = [this](AircraftId aircraftId, std::uint32_t point) {
        m_simConnect.TransmitEventEx1(aircraftId, kOpenAircraftDoorsEvent, point + 1, 1,
                                      NewCommandRequest());
    };
    services.closeCargoDoor = [this](AircraftId aircraftId, std::uint32_t point) {
        m_simConnect.TransmitEventEx1(aircraftId, kCloseAircraftDoorsEvent, point + 1, 1,
                                      NewCommandRequest());
    };
    services.setRampTarget = [this](AircraftId objectId, double angle) {
        const BaggageLoaderRampTargetWireData data{angle};
        m_simConnect.SetObjectData(kRampTargetDefinition, objectId, 0,
                                   sizeof(data), &data, NewCommandRequest());
    };
    services.requestBaggageGeometry = [this](AircraftId objectId) {
        if (m_geometryRequests.contains(objectId)) return;
        auto request = std::make_unique<GSReqBaggageGeometry>();
        GSReqBaggageGeometry &requestReference = *request;
        m_geometryRequests.emplace(objectId, std::move(request));
        m_simConnect.RequestObjectData(kGeometryDefinition, objectId,
                                       SIMCONNECT_PERIOD_ONCE, 0,
                                       requestReference);
    };
    services.isParentCreated = [this](AircraftId parentObjectId) {
        return m_createdObjects.contains(parentObjectId);
    };
    return services;
}
} // namespace parking_services
