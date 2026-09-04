#include "GroundServicesThread.h"

#include "AircraftTrackerThread.h"
#include "GSCommon.h"
#include "GSObject.h"
#include "SimObjectPositioning.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace parking_services
{
using namespace std::chrono_literals;

namespace
{
constexpr double kSpawnSpeedKnots = 1.0;
constexpr double kRemovalSpeedKnots = 2.0;

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

GroundServicesThread::GroundServicesThread(ISimConnectHandler &simConnect,
                                           AircraftTrackerThread &aircraftTracker,
                                           AnimationThread &animation,
                                           GroundServicesConfig configuration)
    : m_simConnect(simConnect), m_aircraftTracker(aircraftTracker),
      m_animation(animation), m_configuration(std::move(configuration))
{
    for (const std::string &message : m_configuration.StartupMessages()) {
        GSLog(message);
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
        RequestRemovalOfAllServices();
        GSLog("Reset ground-service state and requested removal of all created objects.");
    });
}

void GroundServicesThread::RepositionStaticObject(
    AircraftId objectId, double x, double y, double z,
    double headingDegrees)
{
    Post([this, objectId, x, y, z, headingDegrees] {
        HandleRepositionStaticObject(objectId, x, y, z, headingDegrees);
    });
}

void GroundServicesThread::FindClosestRootObject(std::string family)
{
    Post([this, family = std::move(family)] {
        HandleFindClosestRootObject(family);
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
    std::uint64_t evaluatedSnapshotRevision{};
    while (!stopToken.stop_requested()) {
        ProcessCommands();
        PollRequests();
        const auto now = std::chrono::steady_clock::now();
        if (m_connected) {
            MaintainObjects(now);
            const std::uint64_t snapshotRevision =
                m_aircraftTracker.SnapshotRevision();
            if (snapshotRevision != evaluatedSnapshotRevision) {
                EvaluateTrackedAircraft();
                evaluatedSnapshotRevision = snapshotRevision;
            }
        }
        std::unique_lock lock(m_commandMutex);
        m_wake.wait_for(lock, stopToken, 500ms, [this] { return !m_commands.empty(); });
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
    m_animation.InitializeSimConnect();
    CollectFinishedCommandRequests();
    m_nextObjectDataDefinition = 10'000;
    m_nextObjectClientEvent = 10'000;
    m_findPoseDefinition = static_cast<SIMCONNECT_DATA_DEFINITION_ID>(
        m_nextObjectDataDefinition++);
    m_simConnect.AddDatum(m_findPoseDefinition, "PLANE LATITUDE", "degrees",
                          SIMCONNECT_DATATYPE_FLOAT64, NewCommandRequest());
    m_simConnect.AddDatum(m_findPoseDefinition, "PLANE LONGITUDE", "degrees",
                          SIMCONNECT_DATATYPE_FLOAT64, NewCommandRequest());
    m_simConnect.AddDatum(m_findPoseDefinition, "PLANE ALTITUDE", "feet",
                          SIMCONNECT_DATATYPE_FLOAT64, NewCommandRequest());
    m_simConnect.AddDatum(m_findPoseDefinition,
                          "PLANE HEADING DEGREES TRUE", "degrees",
                          SIMCONNECT_DATATYPE_FLOAT64, NewCommandRequest());
}

void GroundServicesThread::PollRequests()
{
    CollectFinishedCommandRequests();
    CollectRetiredObjects();
    PollFindOperation();

    std::vector<std::uint64_t> completedCreates;
    for (const auto &[token, operation] : m_createRequests) {
        if (operation.request->IsFinished()) completedCreates.push_back(token);
    }
    for (const std::uint64_t token : completedCreates) {
        const auto operationIt = m_createRequests.find(token);
        if (operationIt == m_createRequests.end()) continue;
        const AircraftId aircraftId = operationIt->second.aircraftId;
        GSObject *object = operationIt->second.object;
        const DWORD objectId = operationIt->second.request->ObjectId();
        m_createRequests.erase(operationIt);

        if (!object) {
            if (objectId != 0) {
                m_simConnect.RemoveObject(objectId, NewCommandRequest());
            }
            DecrementPendingCreations(aircraftId);
            continue;
        }
        object->OnCreated(m_services, objectId);
        DecrementPendingCreations(aircraftId);
    }
    TryFinalizeDeferredRemovals();
}

void GroundServicesThread::PollFindOperation()
{
    if (!m_findOperation) return;
    if (!std::ranges::all_of(m_findOperation->rows, [](const FindRow &row) {
            return !row.request || row.request->IsFinished();
        })) {
        return;
    }

    std::ostringstream output;
    output << std::fixed << std::setprecision(3)
           << "Closest " << m_findOperation->family << " root is "
           << m_findOperation->distanceMeters << " m from the user aircraft:";
    for (const FindRow &row : m_findOperation->rows) {
        output << '\n' << std::string(row.depth * 2, ' ')
               << "ID=" << row.objectId << " family=" << row.family;
        if (row.root) {
            output << " location=root heading=" << row.storedWorldHeading
                   << " (stored world)";
        } else {
            output << " location=[" << row.relativeX << ','
                   << row.relativeY << ',' << row.relativeZ
                   << "] heading=" << row.relativeHeading;
        }

        const GSObjectPoseResult pose = row.request
                                            ? row.request->Result()
                                            : GSObjectPoseResult{};
        if (!pose.succeeded) {
            output << " real=unavailable";
            continue;
        }
        output << " real=[lat=" << std::setprecision(8) << pose.latitude
               << ",lon=" << pose.longitude << std::setprecision(3)
               << ",alt-ft=" << pose.altitudeFeet
               << "] real-heading=" << pose.headingDegrees;
    }
    GSPrint(output.str());
    m_findOperation.reset();
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

void GroundServicesThread::CollectRetiredObjects()
{
    std::erase_if(m_retiredObjects, [](const auto &object) {
        return object->ReadyToDestroy();
    });
}

// Drives one maintenance tick for every live object. Concrete behavior stays
// behind the GSObject API.
void GroundServicesThread::MaintainObjects(std::chrono::steady_clock::time_point now)
{
    std::vector<std::uint64_t> retired;
    for (auto &[token, object] : m_rootObjects) {
        object->MaintainTree(m_services, now);
        if (object->Retired()) {
            retired.push_back(token);
        }
    }
    for (const std::uint64_t token : retired) {
        const auto object = m_rootObjects.find(token);
        if (object == m_rootObjects.end()) continue;
        if (!object->second->ReadyToDestroy()) {
            m_retiredObjects.push_back(std::move(object->second));
        }
        m_rootObjects.erase(object);
    }
    if (!retired.empty()) PublishStatus();
    TryFinalizeDeferredRemovals();
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
    for (const auto &[token, object] : m_rootObjects) {
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
    GSLog("Applied " + std::to_string(m_serviceRequestBuffer.size()) +
          " configured " + std::string(AircraftSizeCategoryName(*category)) +
          " service request(s) to aircraft " + std::to_string(aircraft.objectId) +
          " (wingspan " + std::to_string(aircraft.wingSpanMeters) + " m.");
    PublishStatus();
}

// Builds the GSObject for one request (dispatching on specialType inside the
// factory), resolves its placement, and starts the create. The concrete
// subclass encapsulates all specialized placement and setup.
void GroundServicesThread::QueueService(const AircraftSnapshot &aircraft,
                                        const GroundServiceRequest &request)
{
    const std::uint64_t token = m_nextCreateToken++;
    auto object = GSObject::Create(aircraft, request);
    if (!object) return;
    if (!object->PreparePlacement(m_services)) {
        GSLog("Skipped " + request.object.family + " for aircraft " +
              std::to_string(aircraft.objectId) + ": no valid placement was found.");
        return;
    }
    object->ConfigureSimConnect(m_services);
    GSObject &root = *object;
    m_rootObjects.emplace(token, std::move(object));
    BeginCreate(token, root);
}
// Starts asynchronous creation for a GSObject that already knows its pose. The
// result is delivered through the generic OnCreated lifecycle hook.
void GroundServicesThread::BeginCreate(std::uint64_t token, GSObject &object)
{
    const AircraftId aircraftId = object.AircraftObjectId();
    m_pendingCreations[aircraftId]++;
    PublishStatus();
    auto request = std::make_unique<GSReqCreateObject>();
    GSReqCreateObject &requestReference = *request;
    m_createRequests.emplace(
        token, CreateOperation{aircraftId, &object, std::move(request)});
    m_simConnect.CreateObject(object.Object().title,
                              GSObject::ToInitialPosition(object.Pose()),
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
}

void GroundServicesThread::QueueAttachments(GSObject &parent,
                                            const GSObject::GSObjectPos &parentPose,
                                            const std::vector<GroundServiceObject> &attachments)
{
    for (const GroundServiceObject &attachment : attachments) {
        const std::uint64_t token = m_nextCreateToken++;
        auto object = GSObject::CreateAttachment(parent.Aircraft(), attachment);
        if (!object) continue;
        object->PrepareAttachment(parentPose);
        object->ConfigureSimConnect(m_services);
        GSObject &child = parent.AddChild(std::move(object));
        BeginCreate(token, child);
    }
}

// Shared completion (GSObject::Finish): construct the complete generic animation
// and hand it to AnimationThread, then queue any child attachments.
void GroundServicesThread::FinalizeObject(GSObject *object,
                                          const GSObject::GSObjectPos &actualPose)
{
    if (object->Object().animation) {
        std::vector<AnimationCoordinate> coordinates =
            object->MovementCoordinates();
        if (coordinates.empty()) {
            coordinates.push_back({actualPose.latitude, actualPose.longitude,
                                   actualPose.altitudeFeet,
                                   actualPose.headingDegrees});
        }
        m_animation.AddObject(std::make_unique<AnimatedObject>(
            object->ObjectId(), *object->Object().animation,
            std::move(coordinates),
            object->MovementSpeedMetersPerSecond()));
    }
    QueueAttachments(*object, actualPose, object->Object().attachments);
}

void GroundServicesThread::RequestRemovalOfAllServices()
{
    std::set<AircraftId> aircraftIds = m_configuredAircraft;
    for (const auto &[token, object] : m_rootObjects) {
        static_cast<void>(token);
        aircraftIds.insert(object->AircraftObjectId());
    }
    for (const auto &[aircraftId, count] : m_pendingCreations) {
        static_cast<void>(count);
        aircraftIds.insert(aircraftId);
    }
    for (const AircraftId aircraftId : aircraftIds) {
        RemoveForAircraft(aircraftId);
    }
}

void GroundServicesThread::RemoveForAircraft(AircraftId aircraftId)
{
    m_configuredAircraft.erase(aircraftId);
    if (!AircraftWorkComplete(aircraftId)) {
        if (m_deferredAircraftRemovals.insert(aircraftId).second) {
            GSLog("Deferring removal of aircraft " + std::to_string(aircraftId) +
                  " until its ground-service work is complete.");
        }
        PublishStatus();
        return;
    }
    m_deferredAircraftRemovals.erase(aircraftId);
    RemoveResolvedObjects(aircraftId);
    PublishStatus();
}

// Removes only this aircraft's objects whose create has already resolved
// (created objects via RemoveObject; failed roots are dropped from m_rootObjects).
// In-flight objects are left untouched so their create can still complete.
void GroundServicesThread::RemoveResolvedObjects(AircraftId aircraftId)
{
    std::vector<std::uint64_t> tokens;
    for (auto &[token, object] : m_rootObjects) {
        if (object->AircraftObjectId() != aircraftId) continue;
        if (!object->Resolved()) continue;
        tokens.push_back(token);
    }
    for (const std::uint64_t token : tokens) {
        const auto it = m_rootObjects.find(token);
        if (it == m_rootObjects.end()) continue;
        const AircraftId objectId = it->second->ObjectId();
        if (objectId != 0) {
            RemoveObject(objectId,
                         !m_deferredObjectRemovals.contains(objectId));
        } else {
            if (!it->second->ReadyToDestroy()) {
                m_retiredObjects.push_back(std::move(it->second));
            }
            m_rootObjects.erase(it);
        }
    }
}

// Completes a previously deferred removal only after every object belonging to
// the aircraft has finished its current lifecycle work.
void GroundServicesThread::FinalizeDeferredRemoval(AircraftId aircraftId)
{
    if (!m_deferredAircraftRemovals.contains(aircraftId) ||
        !AircraftWorkComplete(aircraftId)) {
        return;
    }
    m_deferredAircraftRemovals.erase(aircraftId);
    RemoveResolvedObjects(aircraftId);
    m_configuredAircraft.erase(aircraftId);
    GSLog("Completed deferred removal of services for aircraft " +
          std::to_string(aircraftId) + ".");
    PublishStatus();
}

void GroundServicesThread::TryFinalizeDeferredRemovals()
{
    std::vector<AircraftId> aircraftIds(m_deferredAircraftRemovals.begin(),
                                        m_deferredAircraftRemovals.end());
    for (const AircraftId aircraftId : aircraftIds) {
        FinalizeDeferredRemoval(aircraftId);
    }

    std::vector<AircraftId> objectIds(m_deferredObjectRemovals.begin(),
                                      m_deferredObjectRemovals.end());
    for (const AircraftId objectId : objectIds) {
        const auto owner = m_aircraftByObject.find(objectId);
        if (owner == m_aircraftByObject.end()) {
            m_deferredObjectRemovals.erase(objectId);
            continue;
        }
        if (!AircraftWorkComplete(owner->second)) continue;
        m_deferredObjectRemovals.erase(objectId);
        RemoveObject(objectId, /*requestSimulatorRemoval=*/false);
        GSLog("Finished pending work for externally removed ground ObjectID " +
              std::to_string(objectId) + " and retired its object subtree.");
        PublishStatus();
    }
}

bool GroundServicesThread::AircraftWorkComplete(AircraftId aircraftId) const
{
    if (PendingCount(aircraftId) != 0) return false;
    return std::ranges::all_of(m_rootObjects, [aircraftId](const auto &entry) {
        const GSObject &object = *entry.second;
        return object.AircraftObjectId() != aircraftId ||
               object.ReadyForRemoval();
    });
}

std::size_t GroundServicesThread::PendingCount(AircraftId aircraftId) const
{
    const auto it = m_pendingCreations.find(aircraftId);
    return it == m_pendingCreations.end() ? 0 : it->second;
}

// Called when a creation resolves. Draining the create count permits deferred
// removal only if subtype-specific object work has also finished.
void GroundServicesThread::DecrementPendingCreations(AircraftId aircraftId)
{
    const auto it = m_pendingCreations.find(aircraftId);
    if (it == m_pendingCreations.end()) return;
    if (--it->second == 0) {
        m_pendingCreations.erase(it);
        FinalizeDeferredRemoval(aircraftId);
    }
    PublishStatus();
}

// Removes one created object (and any children first) through the generic
// GSObject removal hook.
void GroundServicesThread::RemoveObject(AircraftId objectId, bool requestSimulatorRemoval)
{
    if (objectId == 0) return;
    auto root = m_rootObjects.end();
    GSObject *object = nullptr;
    for (auto entry = m_rootObjects.begin(); entry != m_rootObjects.end(); ++entry) {
        if (GSObject *found = entry->second->FindByObjectId(objectId)) {
            root = entry;
            object = found;
            break;
        }
    }
    if (!object) return;

    RemoveObjectState(*object, requestSimulatorRemoval);

    std::unique_ptr<GSObject> removed;
    if (root->second.get() == object) {
        removed = std::move(root->second);
        m_rootObjects.erase(root);
    } else {
        removed = root->second->ReleaseDescendant(objectId);
    }
    if (removed && !removed->ReadyToDestroy()) {
        m_retiredObjects.push_back(std::move(removed));
    }
}

void GroundServicesThread::RemoveObjectState(GSObject &object,
                                              bool requestSimulatorRemoval)
{
    for (const auto &child : object.Children()) {
        const AircraftId childId = child->ObjectId();
        RemoveObjectState(*child,
                          childId != 0 &&
                              !m_deferredObjectRemovals.contains(childId));
    }

    const AircraftId objectId = object.ObjectId();
    if (objectId == 0) return;
    m_deferredObjectRemovals.erase(objectId);
    object.OnRemoved(m_services);
    m_animation.RemoveObject(objectId);
    if (requestSimulatorRemoval) {
        m_simConnect.RemoveObject(objectId, NewCommandRequest());
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
        std::ranges::any_of(m_rootObjects, [objectId](const auto &entry) {
            return entry.second->AircraftObjectId() == objectId;
        });
    if (isTrackedAircraft) {
        GSLog("MSFS removed tracked aircraft ObjectID " + std::to_string(objectId) +
              "; removing its pending and created ground services.");
        RemoveForAircraft(objectId);
        return;
    }

    if (m_createdObjects.contains(objectId)) {
        m_deferredObjectRemovals.insert(objectId);
        GSLog("MSFS removed created ground ObjectID " + std::to_string(objectId) +
              "; its pending work will finish before the object subtree is retired.");
    }
    PublishStatus();
}

void GroundServicesThread::HandleRepositionStaticObject(
    AircraftId objectId, double x, double y, double z,
    double headingDegrees)
{
    GSObject *object = nullptr;
    for (const auto &[token, root] : m_rootObjects) {
        static_cast<void>(token);
        if (GSObject *found = root->FindByObjectId(objectId)) {
            object = found;
            break;
        }
    }

    if (!object) {
        GSPrint("Cannot reposition ObjectID " + std::to_string(objectId) +
                ": it is not a managed ground-service object.");
        return;
    }
    if (!object->RepositionRelative(m_services, x, y, z,
                                    headingDegrees)) {
        GSPrint("Cannot reposition ObjectID " + std::to_string(objectId) +
                ": it is not a static object.");
        return;
    }

    GSPrint("Requested reposition of static ObjectID " +
            std::to_string(objectId) + " to parent-relative XYZH [" +
            std::to_string(x) + ", " + std::to_string(y) + ", " +
            std::to_string(z) + ", " + std::to_string(headingDegrees) +
            "].");
}

void GroundServicesThread::HandleFindClosestRootObject(
    const std::string &family)
{
    if (m_findOperation) {
        GSPrint("A find request is already in progress.");
        return;
    }

    ApproximateUserPosition userPosition{};
    if (!m_aircraftTracker.TryGetApproximateUserPosition(userPosition)) {
        GSPrint("Cannot find family " + family +
                ": the user-aircraft position is not available.");
        return;
    }

    const GSObject *closest = nullptr;
    double closestDistance = std::numeric_limits<double>::max();
    for (const auto &[token, root] : m_rootObjects) {
        static_cast<void>(token);
        if (root->ObjectId() == 0 ||
            !EqualIgnoreCase(root->Object().family, family)) {
            continue;
        }
        const double distance = DistanceMeters(
            userPosition.latitude, userPosition.longitude,
            root->Pose().latitude, root->Pose().longitude);
        if (distance < closestDistance) {
            closest = root.get();
            closestDistance = distance;
        }
    }

    if (!closest) {
        GSPrint("No managed root object belongs to family " + family + ".");
        return;
    }

    auto operation = std::make_unique<FindOperation>();
    operation->family = closest->Object().family;
    operation->distanceMeters = closestDistance;

    std::function<void(const GSObject &, std::size_t, bool)> appendRows;
    appendRows = [&](const GSObject &object, std::size_t depth, bool root) {
        FindRow row{};
        row.objectId = object.ObjectId();
        row.family = object.Object().family;
        row.relativeX = object.Object().parentX;
        row.relativeY = object.Object().parentY;
        row.relativeZ = object.Object().parentZ;
        row.relativeHeading = object.Object().parentHeadingDegrees;
        row.storedWorldHeading = object.Pose().headingDegrees;
        row.depth = depth;
        row.root = root;
        if (row.objectId != 0) {
            row.request = std::make_unique<GSReqObjectPose>();
            m_simConnect.RequestObjectData(
                m_findPoseDefinition, row.objectId, SIMCONNECT_PERIOD_ONCE, 0,
                *row.request);
        }
        operation->rows.push_back(std::move(row));
        for (const auto &child : object.Children()) {
            appendRows(*child, depth + 1, false);
        }
    };
    appendRows(*closest, 0, true);
    m_findOperation = std::move(operation);
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
        GSLog(message);
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
    // Forced teardown invalidates runtime object ownership. Leave create
    // requests alive, but detach their non-owning object pointers first.
    for (auto &[token, operation] : m_createRequests) {
        static_cast<void>(token);
        operation.object = nullptr;
    }
    std::vector<AircraftId> objects;
    objects.assign(m_createdObjects.begin(), m_createdObjects.end());
    for (const AircraftId objectId : objects) {
        if (m_createdObjects.contains(objectId)) {
            RemoveObject(objectId,
                         !m_deferredObjectRemovals.contains(objectId));
        }
    }
    for (auto &[token, object] : m_rootObjects) {
        static_cast<void>(token);
        if (!object->ReadyToDestroy()) {
            m_retiredObjects.push_back(std::move(object));
        }
    }
    m_rootObjects.clear();
    m_configuredAircraft.clear();
    m_objectsByAircraft.clear();
    m_aircraftByObject.clear();
    m_pendingCreations.clear();
    m_deferredAircraftRemovals.clear();
    m_deferredObjectRemovals.clear();
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
// Generic services shared by every GSObject. No concrete object type is named
// here; specialized SimConnect behavior remains inside the subclass.
GSObjectServices GroundServicesThread::MakeServices()
{
    GSObjectServices services;
    services.simConnect = &m_simConnect;
    services.allocateDataDefinition = [this] {
        return static_cast<SIMCONNECT_DATA_DEFINITION_ID>(
            m_nextObjectDataDefinition++);
    };
    services.allocateClientEvent = [this] {
        return static_cast<SIMCONNECT_CLIENT_EVENT_ID>(
            m_nextObjectClientEvent++);
    };
    services.registerObject = [this](GSObject *object, AircraftId objectId,
                                     const GSObject::GSObjectPos &pose) {
        if (object) RegisterObject(object, objectId, pose);
    };
    services.finalize = [this](GSObject *object, const GSObject::GSObjectPos &pose) {
        if (object) FinalizeObject(object, pose);
    };
    return services;
}
} // namespace parking_services
