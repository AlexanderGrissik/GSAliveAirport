#include "SimConnectThread.h"

#include "SimConnectIds.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <utility>

namespace parking_services
{
using namespace std::chrono_literals;

namespace
{
constexpr DWORD kScanRadiusMeters = 5'000;
constexpr auto kRequestTimeout = 8s;

template <std::size_t Size> std::string FixedString(const std::array<char, Size> &value)
{
    const auto end = std::find(value.begin(), value.end(), '\0');
    return {value.data(), static_cast<std::size_t>(end - value.begin())};
}

template <typename Payload, typename Entry>
std::optional<Payload> ReadPayload(const Entry &entry, DWORD messageSize)
{
    const auto *entryBytes = reinterpret_cast<const BYTE *>(&entry);
    const auto *payloadBytes = reinterpret_cast<const BYTE *>(&entry.dwData);
    const std::size_t offset = static_cast<std::size_t>(payloadBytes - entryBytes);
    const std::size_t received = entry.dwSize != 0
                                     ? (std::min)(static_cast<std::size_t>(entry.dwSize),
                                                  static_cast<std::size_t>(messageSize))
                                     : static_cast<std::size_t>(messageSize);
    if (received < offset + sizeof(Payload)) {
        return std::nullopt;
    }
    Payload payload{};
    std::memcpy(&payload, payloadBytes, sizeof(payload));
    return payload;
}

#pragma pack(push, 1)
struct AircraftWireData
{
    std::array<char, 256> title{};
    std::array<char, 256> atcId{};
    std::array<char, 256> atcAirline{};
    std::array<char, 256> atcFlightNumber{};
    double latitude{};
    double longitude{};
    double altitudeFeet{};
    double groundAltitudeFeet{};
    double headingDegrees{};
    double groundSpeedKnots{};
    double wingSpanMeters{};
    std::int32_t onGround{};
    std::int32_t isUser{};
    std::array<char, 256> currentAirport{};
    std::array<char, 256> assignedParking{};
    std::array<char, 256> assignedRunway{};
    std::array<char, 256> fromAirport{};
    std::array<char, 256> toAirport{};
    std::int32_t etdSeconds{};
    std::int32_t etaSeconds{};
    std::array<char, 256> trafficState{};
    std::int32_t isIfr{};
    std::int32_t numberOfEngines{};
    std::array<std::int32_t, 4> engineCombustion{};
    std::array<std::int32_t, 4> engineStarterActive{};
    std::array<double, 4> engineN1Percent{};
    std::int32_t lightBeacon{};
    std::int32_t lightNav{};
    std::int32_t lightTaxi{};
    std::int32_t lightStrobe{};
    std::int32_t parkingBrake{};
    std::int32_t pushbackAttached{};
    std::int32_t pushbackWait{};
    std::int32_t transponderState{};
};

struct GroundWireData
{
    std::array<char, 256> title{};
    double latitude{};
    double longitude{};
    double groundSpeedKnots{};
};

struct ProbeWireData
{
    std::array<char, 256> title{};
    double groundSpeedKnots{};
    double velocityBodyXMetersPerSecond{};
    double velocityBodyYMetersPerSecond{};
    double velocityBodyZMetersPerSecond{};
    double headingDegrees{};
    double latitude{};
    double longitude{};
    double altitudeFeet{};
    std::int32_t onGround{};
};

struct AnimationWireData
{
    double latitude{};
    double longitude{};
    double altitudeFeet{};
    double headingDegrees{};
    double animationFrame{};
};
#pragma pack(pop)

static_assert(sizeof(AircraftWireData) == 2736);
static_assert(sizeof(GroundWireData) == 280);
static_assert(sizeof(ProbeWireData) == 324);
static_assert(sizeof(AnimationWireData) == 40);

AircraftSnapshot ToAircraft(DWORD objectId, const AircraftWireData &data)
{
    AircraftSnapshot aircraft{};
    aircraft.objectId = objectId;
    aircraft.title = FixedString(data.title);
    aircraft.atcId = FixedString(data.atcId);
    aircraft.atcAirline = FixedString(data.atcAirline);
    aircraft.atcFlightNumber = FixedString(data.atcFlightNumber);
    aircraft.latitude = data.latitude;
    aircraft.longitude = data.longitude;
    aircraft.altitudeFeet = data.altitudeFeet;
    aircraft.groundAltitudeFeet = data.groundAltitudeFeet;
    aircraft.headingDegrees = data.headingDegrees;
    aircraft.groundSpeedKnots = data.groundSpeedKnots;
    aircraft.wingSpanMeters = data.wingSpanMeters;
    aircraft.onGround = data.onGround != 0;
    aircraft.isUser = data.isUser != 0;
    aircraft.currentAirport = FixedString(data.currentAirport);
    aircraft.assignedParking = FixedString(data.assignedParking);
    aircraft.assignedRunway = FixedString(data.assignedRunway);
    aircraft.fromAirport = FixedString(data.fromAirport);
    aircraft.toAirport = FixedString(data.toAirport);
    aircraft.etdSeconds = data.etdSeconds;
    aircraft.etaSeconds = data.etaSeconds;
    aircraft.trafficState = FixedString(data.trafficState);
    aircraft.isIfr = data.isIfr != 0;
    aircraft.numberOfEngines = data.numberOfEngines;
    for (std::size_t index = 0; index < aircraft.engineCombustion.size(); ++index) {
        aircraft.engineCombustion[index] = data.engineCombustion[index];
        aircraft.engineStarterActive[index] = data.engineStarterActive[index];
        aircraft.engineN1Percent[index] = data.engineN1Percent[index];
    }
    aircraft.lightBeacon = data.lightBeacon != 0;
    aircraft.lightNav = data.lightNav != 0;
    aircraft.lightTaxi = data.lightTaxi != 0;
    aircraft.lightStrobe = data.lightStrobe != 0;
    aircraft.parkingBrake = data.parkingBrake != 0;
    aircraft.pushbackAttached = data.pushbackAttached != 0;
    aircraft.pushbackWait = data.pushbackWait != 0;
    aircraft.transponderState = data.transponderState;
    return aircraft;
}
} // namespace

struct SimConnectThread::PendingAircraftScan
{
    DWORD requestId{};
    AircraftScanCallback callback;
    std::map<DWORD, AircraftWireData> batch;
    std::chrono::steady_clock::time_point deadline{};
};

struct SimConnectThread::PendingGroundScan
{
    DWORD requestId{};
    std::shared_ptr<std::promise<GroundScanResult>> promise;
    std::map<DWORD, GroundWireData> batch;
    std::chrono::steady_clock::time_point deadline{};
};

struct SimConnectThread::PendingCreate
{
    ObjectCreatedCallback callback;
    std::chrono::steady_clock::time_point deadline{};
};

struct SimConnectThread::PendingProbe
{
    DWORD requestId{};
    DWORD objectId{};
    ProbeSampleCallback callback;
    std::chrono::steady_clock::time_point started{};
};

SimConnectThread::SimConnectThread(LogSink log)
    : m_log(std::move(log)), m_session(m_log), m_catalog(m_log),
      m_thread(&SimConnectThread::SimConnectLoop, this)
{
}

SimConnectThread::~SimConnectThread()
{
    Stop();
}

void SimConnectThread::Stop()
{
    if (!m_thread.joinable()) return;
    m_thread.request_stop();
    m_wake.notify_all();
    m_thread.join();
}

void SimConnectThread::RequestAircraftScan(AircraftScanCallback callback)
{
    Post([this, callback = std::move(callback)]() mutable {
        BeginAircraftScan(std::move(callback));
    });
}

std::future<GroundScanResult> SimConnectThread::RequestGroundObjects()
{
    auto promise = std::make_shared<std::promise<GroundScanResult>>();
    auto future = promise->get_future();
    Post([this, promise] { BeginGroundScan(promise); });
    return future;
}

void SimConnectThread::CreateObject(std::string title,
                                    SIMCONNECT_DATA_INITPOSITION position,
                                    ObjectCreatedCallback callback)
{
    Post([this, title = std::move(title), position, callback = std::move(callback)]() mutable {
        BeginCreate(std::move(title), position, std::move(callback));
    });
}

void SimConnectThread::RemoveObject(DWORD objectId)
{
    Post([this, objectId] {
        if (m_session.IsConnected()) {
            m_session.RemoveObject(objectId, m_session.NextRequestId());
        }
    });
}

void SimConnectThread::FreezeObject(DWORD objectId)
{
    Post([this, objectId] {
        if (!m_session.IsConnected()) return;
        m_session.TransmitEvent(objectId, EventFreezeLatitudeLongitude, 1);
        m_session.TransmitEvent(objectId, EventFreezeAltitude, 1);
        m_session.TransmitEvent(objectId, EventFreezeAttitude, 1);
    });
}

void SimConnectThread::PublishAnimationUpdates(
    const std::vector<AnimationUpdate> &updates)
{
    if (!m_connected.load()) return;
    {
        std::scoped_lock lock(m_animationMutex);
        for (const AnimationUpdate &update : updates) {
            m_latestAnimationUpdates[update.objectId] = update;
        }
    }
    m_wake.notify_all();
}

void SimConnectThread::CancelAnimationObject(DWORD objectId)
{
    std::scoped_lock lock(m_animationMutex);
    m_latestAnimationUpdates.erase(objectId);
}

void SimConnectThread::StartAnimationProbe(DWORD objectId, ProbeSampleCallback callback)
{
    Post([this, objectId, callback = std::move(callback)]() mutable {
        BeginProbe(objectId, std::move(callback));
    });
}

void SimConnectThread::StopAnimationProbe()
{
    Post([this] { EndProbe(); });
}

void SimConnectThread::RequestCatalog()
{
    Post([this] {
        m_catalog.Request(m_session, [this](std::vector<std::string> titles) {
            PublishAvailableSimObjectTitles(std::move(titles));
        });
    });
}

bool SimConnectThread::FillAvailableSimObjectTitles(
    std::vector<std::string> &destination) const
{
    std::scoped_lock lock(m_catalogSnapshotMutex);
    destination.clear();
    if (!m_catalogSnapshotReady) return false;
    destination = m_availableSimObjectTitles;
    return true;
}

void SimConnectThread::SubscribeObjectRemoved(ObjectRemovedCallback callback)
{
    std::scoped_lock lock(m_subscriberMutex);
    m_objectRemovedCallbacks.push_back(std::move(callback));
}

void SimConnectThread::SubscribeConnection(ConnectionCallback callback)
{
    const bool connected = m_connected.load();
    {
        std::scoped_lock lock(m_subscriberMutex);
        m_connectionCallbacks.push_back(callback);
    }
    if (connected) callback(true);
}

bool SimConnectThread::IsConnected() const
{
    return m_connected.load();
}

void SimConnectThread::SimConnectLoop(std::stop_token stopToken, SimConnectThread *self)
{
    self->RunLoop(stopToken);
}

void SimConnectThread::RunLoop(std::stop_token stopToken)
{
    auto nextConnectionAttempt = std::chrono::steady_clock::now();
    while (!stopToken.stop_requested()) {
        const auto now = std::chrono::steady_clock::now();
        if (!m_session.IsConnected() && now >= nextConnectionAttempt) {
            if (!Connect()) nextConnectionAttempt = now + 2s;
        }

        if (m_session.IsConnected()) {
            const HRESULT dispatch = m_session.Dispatch();
            if (m_disconnectRequested || FAILED(dispatch)) {
                if (FAILED(dispatch) && !m_disconnectRequested) {
                    m_log("SimConnect dispatch failed; reconnecting.");
                }
                Disconnect();
                nextConnectionAttempt = now + 2s;
            }
        }

        ProcessCommands();
        ProcessAnimationUpdates();
        MaintainPendingRequests();

        std::unique_lock lock(m_commandMutex);
        m_wake.wait_for(lock, stopToken, 5ms, [this] { return !m_commands.empty(); });
    }
    ProcessCommands();
    Disconnect();
}

void SimConnectThread::Post(std::function<void()> command)
{
    {
        std::scoped_lock lock(m_commandMutex);
        m_commands.push_back(std::move(command));
    }
    m_wake.notify_all();
}

void SimConnectThread::ProcessCommands()
{
    std::deque<std::function<void()>> commands;
    {
        std::scoped_lock lock(m_commandMutex);
        commands.swap(m_commands);
    }
    for (auto &command : commands) command();
}

void SimConnectThread::ProcessAnimationUpdates()
{
    if (!m_session.IsConnected()) return;
    std::map<DWORD, AnimationUpdate> updates;
    {
        std::scoped_lock lock(m_animationMutex);
        updates.swap(m_latestAnimationUpdates);
    }
    for (const auto &[objectId, update] : updates) {
        const AnimationWireData data{update.latitude, update.longitude, update.altitudeFeet,
                                     update.headingDegrees, update.animationFrame};
        if (!m_session.SetObjectData(DefinitionAnimationUpdate, objectId, 0,
                                     sizeof(data), &data).Succeeded()) {
            m_log("SimConnect rejected an animation update for ObjectID " +
                  std::to_string(objectId) + ".");
        }
    }
}

void SimConnectThread::MaintainPendingRequests()
{
    const auto now = std::chrono::steady_clock::now();
    if (m_aircraftScan && now >= m_aircraftScan->deadline) CompleteAircraftScan(false);
    if (m_groundScan && now >= m_groundScan->deadline) CompleteGroundScan(false);
    std::vector<DWORD> expiredCreates;
    for (const auto &[requestId, create] : m_creates) {
        if (now >= create.deadline) expiredCreates.push_back(requestId);
    }
    for (const DWORD requestId : expiredCreates) CompleteCreate(requestId, 0);
}

bool SimConnectThread::Connect()
{
    if (!m_session.Open(*this)) {
        if (!m_reportedWaiting) {
            m_log("Waiting for Microsoft Flight Simulator 2024...");
            m_reportedWaiting = true;
        }
        return false;
    }
    if (!DefineDataAndEvents()) {
        m_log("Failed to initialize SimConnect definitions; reconnecting.");
        m_session.Close();
        return false;
    }
    m_reportedWaiting = false;
    m_disconnectRequested = false;
    m_connected.store(true);
    m_log("Connected to MSFS 2024.");
    NotifyConnection(true);
    m_catalog.Request(m_session, [this](std::vector<std::string> titles) {
        PublishAvailableSimObjectTitles(std::move(titles));
    });
    return true;
}

void SimConnectThread::Disconnect()
{
    const bool wasConnected = m_connected.exchange(false);
    if (m_aircraftScan) CompleteAircraftScan(false);
    if (m_groundScan) CompleteGroundScan(false);
    std::vector<DWORD> createRequests;
    for (const auto &[requestId, create] : m_creates) createRequests.push_back(requestId);
    for (const DWORD requestId : createRequests) CompleteCreate(requestId, 0);
    if (m_probe) EndProbe();
    m_catalog.Reset();
    {
        std::scoped_lock lock(m_catalogSnapshotMutex);
        m_availableSimObjectTitles.clear();
        m_catalogSnapshotReady = false;
    }
    {
        std::scoped_lock lock(m_animationMutex);
        m_latestAnimationUpdates.clear();
    }
    m_session.Close();
    m_disconnectRequested = false;
    if (wasConnected) NotifyConnection(false);
}

bool SimConnectThread::DefineDataAndEvents()
{
    const auto aircraft = [this](const char *name, const char *units, SIMCONNECT_DATATYPE type) {
        return m_session.AddDatum(DefinitionAircraft, name, units, type);
    };
    const bool aircraftDefined =
        aircraft("TITLE", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
        aircraft("ATC ID", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
        aircraft("ATC AIRLINE", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
        aircraft("ATC FLIGHT NUMBER", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
        aircraft("PLANE LATITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
        aircraft("PLANE LONGITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
        aircraft("PLANE ALTITUDE", "feet", SIMCONNECT_DATATYPE_FLOAT64) &&
        aircraft("GROUND ALTITUDE", "feet", SIMCONNECT_DATATYPE_FLOAT64) &&
        aircraft("PLANE HEADING DEGREES TRUE", "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
        aircraft("GROUND VELOCITY", "knots", SIMCONNECT_DATATYPE_FLOAT64) &&
        aircraft("WING SPAN", "meters", SIMCONNECT_DATATYPE_FLOAT64) &&
        aircraft("SIM ON GROUND", "bool", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("IS USER SIM", "bool", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("AI TRAFFIC CURRENT AIRPORT", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
        aircraft("AI TRAFFIC ASSIGNED PARKING", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
        aircraft("AI TRAFFIC ASSIGNED RUNWAY", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
        aircraft("AI TRAFFIC FROMAIRPORT", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
        aircraft("AI TRAFFIC TOAIRPORT", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
        aircraft("AI TRAFFIC ETD", "seconds", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("AI TRAFFIC ETA", "seconds", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("AI TRAFFIC STATE", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
        aircraft("AI TRAFFIC ISIFR", "bool", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("NUMBER OF ENGINES", "number", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("GENERAL ENG COMBUSTION:1", "bool", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("GENERAL ENG COMBUSTION:2", "bool", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("GENERAL ENG COMBUSTION:3", "bool", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("GENERAL ENG COMBUSTION:4", "bool", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("GENERAL ENG STARTER ACTIVE:1", "bool", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("GENERAL ENG STARTER ACTIVE:2", "bool", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("GENERAL ENG STARTER ACTIVE:3", "bool", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("GENERAL ENG STARTER ACTIVE:4", "bool", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("TURB ENG N1:1", "percent", SIMCONNECT_DATATYPE_FLOAT64) &&
        aircraft("TURB ENG N1:2", "percent", SIMCONNECT_DATATYPE_FLOAT64) &&
        aircraft("TURB ENG N1:3", "percent", SIMCONNECT_DATATYPE_FLOAT64) &&
        aircraft("TURB ENG N1:4", "percent", SIMCONNECT_DATATYPE_FLOAT64) &&
        aircraft("LIGHT BEACON", "bool", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("LIGHT NAV", "bool", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("LIGHT TAXI", "bool", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("LIGHT STROBE", "bool", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("BRAKE PARKING POSITION", "bool", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("PUSHBACK ATTACHED", "bool", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("PUSHBACK WAIT", "bool", SIMCONNECT_DATATYPE_INT32) &&
        aircraft("TRANSPONDER STATE:1", "enum", SIMCONNECT_DATATYPE_INT32);

    const bool groundDefined =
        m_session.AddDatum(DefinitionGround, "TITLE", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
        m_session.AddDatum(DefinitionGround, "PLANE LATITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
        m_session.AddDatum(DefinitionGround, "PLANE LONGITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
        m_session.AddDatum(DefinitionGround, "GROUND VELOCITY", "knots", SIMCONNECT_DATATYPE_FLOAT64);

    const bool animationDefined =
        m_session.AddDatum(DefinitionAnimationUpdate, "PLANE LATITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
        m_session.AddDatum(DefinitionAnimationUpdate, "PLANE LONGITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
        m_session.AddDatum(DefinitionAnimationUpdate, "PLANE ALTITUDE", "feet", SIMCONNECT_DATATYPE_FLOAT64) &&
        m_session.AddDatum(DefinitionAnimationUpdate, "PLANE HEADING DEGREES TRUE", "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
        m_session.AddDatum(DefinitionAnimationUpdate, "VELOCITY BODY Y", "meters per second", SIMCONNECT_DATATYPE_FLOAT64) &&
        m_session.AddDatum(DefinitionAnimationProbe, "TITLE", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
        m_session.AddDatum(DefinitionAnimationProbe, "GROUND VELOCITY", "knots", SIMCONNECT_DATATYPE_FLOAT64) &&
        m_session.AddDatum(DefinitionAnimationProbe, "VELOCITY BODY X", "meters per second", SIMCONNECT_DATATYPE_FLOAT64) &&
        m_session.AddDatum(DefinitionAnimationProbe, "VELOCITY BODY Y", "meters per second", SIMCONNECT_DATATYPE_FLOAT64) &&
        m_session.AddDatum(DefinitionAnimationProbe, "VELOCITY BODY Z", "meters per second", SIMCONNECT_DATATYPE_FLOAT64) &&
        m_session.AddDatum(DefinitionAnimationProbe, "PLANE HEADING DEGREES TRUE", "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
        m_session.AddDatum(DefinitionAnimationProbe, "PLANE LATITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
        m_session.AddDatum(DefinitionAnimationProbe, "PLANE LONGITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
        m_session.AddDatum(DefinitionAnimationProbe, "PLANE ALTITUDE", "feet", SIMCONNECT_DATATYPE_FLOAT64) &&
        m_session.AddDatum(DefinitionAnimationProbe, "SIM ON GROUND", "bool", SIMCONNECT_DATATYPE_INT32);

    const bool eventsDefined =
        m_session.SubscribeSystemEvent(EventSimStart, "SimStart") &&
        m_session.SubscribeSystemEvent(EventSimStop, "SimStop") &&
        m_session.SubscribeSystemEvent(EventObjectAdded, "ObjectAdded") &&
        m_session.SubscribeSystemEvent(EventObjectRemoved, "ObjectRemoved") &&
        m_session.MapClientEvent(EventFreezeLatitudeLongitude, "FREEZE_LATITUDE_LONGITUDE_SET") &&
        m_session.MapClientEvent(EventFreezeAltitude, "FREEZE_ALTITUDE_SET") &&
        m_session.MapClientEvent(EventFreezeAttitude, "FREEZE_ATTITUDE_SET");
    return aircraftDefined && groundDefined && animationDefined && eventsDefined;
}

void SimConnectThread::BeginAircraftScan(AircraftScanCallback callback)
{
    if (!m_session.IsConnected() || m_aircraftScan) {
        callback({false, {}});
        return;
    }
    const DWORD requestId = m_session.NextRequestId();
    const auto send = m_session.RequestObjectsByType(
        requestId, DefinitionAircraft, kScanRadiusMeters, SIMCONNECT_SIMOBJECT_TYPE_AIRCRAFT);
    if (!send.Succeeded()) {
        callback({false, {}});
        return;
    }
    m_aircraftScan = std::make_unique<PendingAircraftScan>(
        PendingAircraftScan{requestId, std::move(callback), {},
                            std::chrono::steady_clock::now() + kRequestTimeout});
    m_session.TrackOperation(send, requestId, " while requesting the aircraft snapshot",
                             [this, requestId] {
                                 if (m_aircraftScan && m_aircraftScan->requestId == requestId) {
                                     CompleteAircraftScan(false);
                                 }
                             });
}

void SimConnectThread::CompleteAircraftScan(bool succeeded)
{
    if (!m_aircraftScan) return;
    auto pending = std::move(m_aircraftScan);
    m_session.CompleteRequest(pending->requestId);
    AircraftScanResult result{succeeded};
    if (succeeded) {
        result.aircraft.reserve(pending->batch.size());
        for (const auto &[objectId, data] : pending->batch) {
            result.aircraft.push_back(ToAircraft(objectId, data));
        }
    }
    pending->callback(std::move(result));
}

void SimConnectThread::BeginGroundScan(
    std::shared_ptr<std::promise<GroundScanResult>> promise)
{
    if (!m_session.IsConnected() || m_groundScan) {
        promise->set_value({false, {}});
        return;
    }
    const DWORD requestId = m_session.NextRequestId();
    const auto send = m_session.RequestObjectsByType(
        requestId, DefinitionGround, kScanRadiusMeters, SIMCONNECT_SIMOBJECT_TYPE_GROUND);
    if (!send.Succeeded()) {
        promise->set_value({false, {}});
        return;
    }
    m_groundScan = std::make_unique<PendingGroundScan>(
        PendingGroundScan{requestId, std::move(promise), {},
                          std::chrono::steady_clock::now() + kRequestTimeout});
    m_session.TrackOperation(send, requestId, " while requesting ground-object diagnostics",
                             [this, requestId] {
                                 if (m_groundScan && m_groundScan->requestId == requestId) {
                                     CompleteGroundScan(false);
                                 }
                             });
}

void SimConnectThread::CompleteGroundScan(bool succeeded)
{
    if (!m_groundScan) return;
    auto pending = std::move(m_groundScan);
    m_session.CompleteRequest(pending->requestId);
    GroundScanResult result{succeeded};
    if (succeeded) {
        result.objects.reserve(pending->batch.size());
        for (const auto &[objectId, data] : pending->batch) {
            result.objects.push_back({objectId, FixedString(data.title), data.latitude,
                                      data.longitude, data.groundSpeedKnots});
        }
    }
    pending->promise->set_value(std::move(result));
}

void SimConnectThread::BeginCreate(std::string title,
                                   SIMCONNECT_DATA_INITPOSITION position,
                                   ObjectCreatedCallback callback)
{
    if (!m_session.IsConnected()) {
        callback(0);
        return;
    }
    const DWORD requestId = m_session.NextRequestId();
    const auto send = m_session.CreateObject(title, position, requestId);
    if (!send.Succeeded()) {
        callback(0);
        return;
    }
    m_creates.emplace(requestId, PendingCreate{std::move(callback),
                                               std::chrono::steady_clock::now() + kRequestTimeout});
    m_session.TrackOperation(send, requestId, " while creating " + title,
                             [this, requestId] { CompleteCreate(requestId, 0); });
}

void SimConnectThread::CompleteCreate(DWORD requestId, DWORD objectId)
{
    const auto pending = m_creates.find(requestId);
    if (pending == m_creates.end()) return;
    auto callback = std::move(pending->second.callback);
    m_creates.erase(pending);
    m_session.CompleteRequest(requestId);
    callback(objectId);
}

void SimConnectThread::BeginProbe(DWORD objectId, ProbeSampleCallback callback)
{
    EndProbe();
    if (!m_session.IsConnected() || objectId == 0) return;
    const DWORD requestId = m_session.NextRequestId();
    const auto send = m_session.RequestObjectData(requestId, DefinitionAnimationProbe,
                                                  objectId, SIMCONNECT_PERIOD_SIM_FRAME, 2);
    if (!send.Succeeded()) return;
    m_probe = std::make_unique<PendingProbe>(
        PendingProbe{requestId, objectId, std::move(callback),
                     std::chrono::steady_clock::now()});
    m_session.TrackOperation(send, requestId,
                             " while starting animation probe for ObjectID " +
                                 std::to_string(objectId),
                             [this, requestId] {
                                 if (m_probe && m_probe->requestId == requestId) m_probe.reset();
                             });
}

void SimConnectThread::EndProbe()
{
    if (!m_probe) return;
    if (m_session.IsConnected()) {
        m_session.RequestObjectData(m_probe->requestId, DefinitionAnimationProbe,
                                    m_probe->objectId, SIMCONNECT_PERIOD_NEVER);
    }
    m_session.CompleteRequest(m_probe->requestId);
    m_probe.reset();
}

void SimConnectThread::OnSimConnectMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    if (!message) return;
    switch (message->dwID) {
    case SIMCONNECT_RECV_ID_QUIT:
        m_log("MSFS closed the SimConnect connection.");
        m_disconnectRequested = true;
        break;
    case SIMCONNECT_RECV_ID_EVENT: {
        const auto &event = *reinterpret_cast<SIMCONNECT_RECV_EVENT *>(message);
        if (event.uEventID == EventSimStart) {
            m_log("Simulation started.");
            NotifyConnection(true);
        } else if (event.uEventID == EventSimStop) {
            m_log("Simulation stopped.");
        }
        break;
    }
    case SIMCONNECT_RECV_ID_EVENT_OBJECT_ADDREMOVE: {
        const auto &event = *reinterpret_cast<SIMCONNECT_RECV_EVENT_OBJECT_ADDREMOVE *>(message);
        if (event.uEventID == EventObjectRemoved) NotifyObjectRemoved(event.dwData);
        break;
    }
    case SIMCONNECT_RECV_ID_SIMOBJECT_DATA_BYTYPE:
        HandleObjectData(*reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE *>(message),
                         messageSize);
        break;
    case SIMCONNECT_RECV_ID_SIMOBJECT_DATA:
        HandleProbeData(*reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA *>(message), messageSize);
        break;
    case SIMCONNECT_RECV_ID_ASSIGNED_OBJECT_ID: {
        const auto &assigned = *reinterpret_cast<SIMCONNECT_RECV_ASSIGNED_OBJECT_ID *>(message);
        CompleteCreate(assigned.dwRequestID, assigned.dwObjectID);
        break;
    }
    case SIMCONNECT_RECV_ID_ENUMERATE_SIMOBJECT_AND_LIVERY_LIST:
        m_catalog.HandleData(
            m_session,
            *reinterpret_cast<SIMCONNECT_RECV_ENUMERATE_SIMOBJECT_AND_LIVERY_LIST *>(message),
            messageSize);
        break;
    case SIMCONNECT_RECV_ID_EXCEPTION:
        HandleException(*reinterpret_cast<SIMCONNECT_RECV_EXCEPTION *>(message));
        break;
    default:
        break;
    }
}

void SimConnectThread::HandleObjectData(
    const SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry, DWORD messageSize)
{
    if (m_aircraftScan && entry.dwRequestID == m_aircraftScan->requestId) {
        const auto payload = ReadPayload<AircraftWireData>(entry, messageSize);
        if (!payload) {
            m_log("Ignored a truncated aircraft data packet.");
            return;
        }
        if (entry.dwentrynumber == 1) m_aircraftScan->batch.clear();
        m_aircraftScan->batch[entry.dwObjectID] = *payload;
        if (entry.dwentrynumber == entry.dwoutof) CompleteAircraftScan(true);
        return;
    }
    if (m_groundScan && entry.dwRequestID == m_groundScan->requestId) {
        const auto payload = ReadPayload<GroundWireData>(entry, messageSize);
        if (!payload) {
            m_log("Ignored a truncated ground-object data packet.");
            return;
        }
        if (entry.dwentrynumber == 1) m_groundScan->batch.clear();
        m_groundScan->batch[entry.dwObjectID] = *payload;
        if (entry.dwentrynumber == entry.dwoutof) CompleteGroundScan(true);
    }
}

void SimConnectThread::HandleProbeData(const SIMCONNECT_RECV_SIMOBJECT_DATA &entry,
                                       DWORD messageSize)
{
    if (!m_probe || entry.dwRequestID != m_probe->requestId ||
        entry.dwObjectID != m_probe->objectId) return;
    const auto payload = ReadPayload<ProbeWireData>(entry, messageSize);
    if (!payload) {
        m_log("Ignored a truncated animation-probe data packet.");
        return;
    }
    m_session.CompleteRequest(m_probe->requestId);
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - m_probe->started).count();
    m_probe->callback({elapsed, entry.dwObjectID, FixedString(payload->title),
                       payload->groundSpeedKnots,
                       payload->velocityBodyXMetersPerSecond,
                       payload->velocityBodyYMetersPerSecond,
                       payload->velocityBodyZMetersPerSecond,
                       payload->headingDegrees, payload->latitude, payload->longitude,
                       payload->altitudeFeet, payload->onGround != 0});
}

void SimConnectThread::HandleException(const SIMCONNECT_RECV_EXCEPTION &exception)
{
    std::ostringstream message;
    message << "SimConnect exception " << exception.dwException
            << " (sendId=" << exception.dwSendID;
    if (exception.dwIndex != SIMCONNECT_RECV_EXCEPTION::UNKNOWN_INDEX) {
        message << ", index=" << exception.dwIndex;
    }
    message << ')';
    if (const auto context = m_session.HandleException(exception.dwSendID)) {
        message << *context;
    }
    m_log(message.str());
}

void SimConnectThread::NotifyObjectRemoved(DWORD objectId)
{
    std::vector<ObjectRemovedCallback> callbacks;
    {
        std::scoped_lock lock(m_subscriberMutex);
        callbacks = m_objectRemovedCallbacks;
    }
    for (const auto &callback : callbacks) callback(objectId);
}

void SimConnectThread::NotifyConnection(bool connected)
{
    std::vector<ConnectionCallback> callbacks;
    {
        std::scoped_lock lock(m_subscriberMutex);
        callbacks = m_connectionCallbacks;
    }
    for (const auto &callback : callbacks) callback(connected);
}

void SimConnectThread::PublishAvailableSimObjectTitles(std::vector<std::string> titles)
{
    std::scoped_lock lock(m_catalogSnapshotMutex);
    m_availableSimObjectTitles = std::move(titles);
    m_catalogSnapshotReady = true;
}
} // namespace parking_services
