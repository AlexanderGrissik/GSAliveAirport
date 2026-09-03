#include "SimConnectThread.h"

#include "SimConnectIds.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <utility>

namespace parking_services
{
using namespace std::chrono_literals;

namespace
{
constexpr std::size_t kInteractivePointProbeCount = 32;
}

SimConnectThread::SimConnectThread(LogSink log) : m_log(std::move(log)) {}

SimConnectThread::~SimConnectThread()
{
    Stop();
}

void SimConnectThread::Start()
{
    if (m_thread.joinable()) return;
    m_thread = std::jthread(&SimConnectThread::SimConnectLoop, this);
}

void SimConnectThread::Stop()
{
    if (!m_thread.joinable()) return;
    m_thread.request_stop();
    m_wake.notify_all();
    m_thread.join();
}

void SimConnectThread::EnumerateObjects(SIMCONNECT_SIMOBJECT_TYPE type,
                                        std::shared_ptr<ISimConnectRequest> request)
{
    if (!request) return;
    Post([this, type, request = std::move(request)] {
        BeginEnumerateObjects(type, request);
    });
}

void SimConnectThread::RequestObjectData(SIMCONNECT_DATA_DEFINITION_ID definition,
                                         DWORD objectId, SIMCONNECT_PERIOD period,
                                         DWORD interval,
                                         std::shared_ptr<ISimConnectRequest> request)
{
    if (!request) return;
    Post([this, definition, objectId, period, interval, request = std::move(request)] {
        BeginRequestObjectData(definition, objectId, period, interval, request);
    });
}

void SimConnectThread::RequestObjectDataByType(
    SIMCONNECT_DATA_DEFINITION_ID definition, DWORD radiusMeters,
    SIMCONNECT_SIMOBJECT_TYPE type, std::shared_ptr<ISimConnectRequest> request)
{
    if (!request) return;
    Post([this, definition, radiusMeters, type, request = std::move(request)] {
        BeginRequestObjectDataByType(definition, radiusMeters, type, request);
    });
}

void SimConnectThread::CreateObject(std::string title,
                                    SIMCONNECT_DATA_INITPOSITION position,
                                    std::shared_ptr<ISimConnectRequest> request)
{
    if (!request) return;
    Post([this, title = std::move(title), position, request = std::move(request)]() mutable {
        BeginCreateObject(std::move(title), position, request);
    });
}

void SimConnectThread::RemoveObject(DWORD objectId,
                                    std::shared_ptr<ISimConnectRequest> request)
{
    if (!request) return;
    Post([this, objectId, request = std::move(request)] {
        BeginRemoveObject(objectId, request);
    });
}

void SimConnectThread::SetObjectData(SIMCONNECT_DATA_DEFINITION_ID definition,
                                     DWORD objectId, DWORD arrayCount,
                                     DWORD elementSize, const void *data,
                                     std::shared_ptr<ISimConnectRequest> request)
{
    if (!request) return;
    std::vector<std::uint8_t> payload;
    if (data && elementSize != 0) {
        const std::size_t count = arrayCount != 0
                                      ? static_cast<std::size_t>(arrayCount) * elementSize
                                      : static_cast<std::size_t>(elementSize);
        const auto *bytes = static_cast<const std::uint8_t *>(data);
        payload.assign(bytes, bytes + count);
    }
    Post([this, definition, objectId, arrayCount, elementSize,
          payload = std::move(payload), request = std::move(request)] {
        BeginSetObjectData(definition, objectId, arrayCount, elementSize, payload, request);
    });
}

void SimConnectThread::AddToDataDefinition(
    SIMCONNECT_DATA_DEFINITION_ID definition, std::string datumName,
    std::string units, SIMCONNECT_DATATYPE type,
    std::shared_ptr<ISimConnectRequest> request)
{
    if (!request) return;
    Post([this, definition, datumName = std::move(datumName),
          units = std::move(units), type, request = std::move(request)]() mutable {
        BeginAddToDataDefinition(definition, std::move(datumName), std::move(units),
                                 type, request);
    });
}

void SimConnectThread::TransmitEvent(DWORD objectId,
                                     SIMCONNECT_CLIENT_EVENT_ID eventId, DWORD data,
                                     std::shared_ptr<ISimConnectRequest> request)
{
    if (!request) return;
    Post([this, objectId, eventId, data, request = std::move(request)] {
        BeginTransmitEvent(objectId, eventId, data, request);
    });
}

void SimConnectThread::TransmitEventEx1(DWORD objectId,
                                        SIMCONNECT_CLIENT_EVENT_ID eventId,
                                        DWORD data0, DWORD data1,
                                        std::shared_ptr<ISimConnectRequest> request)
{
    if (!request) return;
    Post([this, objectId, eventId, data0, data1, request = std::move(request)] {
        BeginTransmitEventEx1(objectId, eventId, data0, data1, request);
    });
}

void SimConnectThread::SimConnectLoop(std::stop_token stopToken,
                                      SimConnectThread *self)
{
    self->RunLoop(stopToken);
}

void SimConnectThread::RunLoop(std::stop_token stopToken)
{
    auto nextConnectionAttempt = std::chrono::steady_clock::now();
    while (!stopToken.stop_requested()) {
        const auto now = std::chrono::steady_clock::now();
        if (!m_handle && now >= nextConnectionAttempt) {
            if (!Connect()) nextConnectionAttempt = now + 2s;
        }

        if (m_handle) {
            const HRESULT dispatch =
                SimConnect_CallDispatch(m_handle, DispatchThunk, this);
            if (m_disconnectRequested || FAILED(dispatch)) {
                if (FAILED(dispatch) && !m_disconnectRequested) {
                    m_log("SimConnect dispatch failed; reconnecting.");
                }
                Disconnect();
                m_disconnectRequested = false;
                nextConnectionAttempt = now + 2s;
            }
        }

        ProcessCommands();
        SweepCompletedRequests();

        std::unique_lock lock(m_commandMutex);
        m_wake.wait_for(lock, stopToken, 5ms,
                        [this] { return !m_commands.empty(); });
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

void SimConnectThread::SweepCompletedRequests()
{
    std::unordered_map<ISimConnectRequest *, std::shared_ptr<ISimConnectRequest>> requests;
    for (const auto &[requestId, request] : m_requestsByRequestId) {
        static_cast<void>(requestId);
        requests.emplace(request.get(), request);
    }
    for (const auto &[sendId, request] : m_requestsBySendId) {
        static_cast<void>(sendId);
        requests.emplace(request.get(), request);
    }
    for (const auto &[address, request] : requests) {
        static_cast<void>(address);
        if (!request->IsComplete()) continue;
        RemoveRequest(request.get());
        request->OnSuccess();
    }
}

bool SimConnectThread::Connect()
{
    if (m_handle) return true;
    HANDLE handle = nullptr;
    if (FAILED(SimConnect_Open(&handle, "ParkingServices", nullptr, 0, nullptr, 0))) {
        if (!m_reportedWaiting) {
            m_log("Waiting for Microsoft Flight Simulator 2024...");
            m_reportedWaiting = true;
        }
        return false;
    }
    m_handle = handle;
    if (!DefineDataAndEvents()) {
        m_log("Failed to initialize SimConnect definitions; reconnecting.");
        SimConnect_Close(m_handle);
        m_handle = nullptr;
        return false;
    }
    m_reportedWaiting = false;
    m_log("Connected to MSFS 2024.");
    NotifyConnection(true);
    return true;
}

void SimConnectThread::Disconnect()
{
    FailPendingRequests();
    if (m_handle) {
        SimConnect_Close(m_handle);
        m_handle = nullptr;
    }
    NotifyConnection(false);
}

bool SimConnectThread::DefineDataAndEvents()
{
    const auto aircraft = [this](const char *name, const char *units,
                                 SIMCONNECT_DATATYPE type) {
        return AddDatum(DefinitionAircraft, name, units, type);
    };
    bool aircraftDefined =
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

    for (std::size_t index = 0;
         aircraftDefined && index < kInteractivePointProbeCount; ++index) {
        const std::string suffix = ":" + std::to_string(index);
        aircraftDefined =
            aircraft(("INTERACTIVE POINT TYPE EX1" + suffix).c_str(), "enum",
                     SIMCONNECT_DATATYPE_INT32) &&
            aircraft(("INTERACTIVE POINT POSX EX1" + suffix).c_str(), "feet",
                     SIMCONNECT_DATATYPE_FLOAT64) &&
            aircraft(("INTERACTIVE POINT POSY EX1" + suffix).c_str(), "feet",
                     SIMCONNECT_DATATYPE_FLOAT64) &&
            aircraft(("INTERACTIVE POINT POSZ EX1" + suffix).c_str(), "feet",
                     SIMCONNECT_DATATYPE_FLOAT64) &&
            aircraft(("INTERACTIVE POINT HEADING EX1" + suffix).c_str(), "degrees",
                     SIMCONNECT_DATATYPE_FLOAT64);
    }

    const bool groundDefined =
        AddDatum(DefinitionGround, "TITLE", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
        AddDatum(DefinitionGround, "PLANE LATITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
        AddDatum(DefinitionGround, "PLANE LONGITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
        AddDatum(DefinitionGround, "GROUND VELOCITY", "knots", SIMCONNECT_DATATYPE_FLOAT64);

    const bool animationDefined =
        AddDatum(DefinitionAnimationUpdate, "PLANE LATITUDE", "degrees",
                 SIMCONNECT_DATATYPE_FLOAT64) &&
        AddDatum(DefinitionAnimationUpdate, "PLANE LONGITUDE", "degrees",
                 SIMCONNECT_DATATYPE_FLOAT64) &&
        AddDatum(DefinitionAnimationUpdate, "PLANE ALTITUDE", "feet",
                 SIMCONNECT_DATATYPE_FLOAT64) &&
        AddDatum(DefinitionAnimationUpdate, "PLANE HEADING DEGREES TRUE", "degrees",
                 SIMCONNECT_DATATYPE_FLOAT64) &&
        AddDatum(DefinitionAnimationUpdate, "VELOCITY BODY Y", "meters per second",
                 SIMCONNECT_DATATYPE_FLOAT64) &&
        AddDatum(DefinitionBaggageLoaderRampTarget, "BAGGAGELOADER ANGLE TARGET",
                 "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
        AddDatum(DefinitionBaggageLoaderGeometry, "BAGGAGELOADER ANGLE CURRENT",
                 "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
        AddDatum(DefinitionBaggageLoaderGeometry, "BAGGAGELOADER END RAMP Y",
                 "meters", SIMCONNECT_DATATYPE_FLOAT64) &&
        AddDatum(DefinitionBaggageLoaderGeometry, "BAGGAGELOADER END RAMP Z",
                 "meters", SIMCONNECT_DATATYPE_FLOAT64) &&
        AddDatum(DefinitionBaggageLoaderGeometry, "BAGGAGELOADER PIVOT Y",
                 "meters", SIMCONNECT_DATATYPE_FLOAT64) &&
        AddDatum(DefinitionBaggageLoaderGeometry, "BAGGAGELOADER PIVOT Z",
                 "meters", SIMCONNECT_DATATYPE_FLOAT64) &&
        AddDatum(DefinitionObjectPosition, "Initial Position", nullptr,
                 SIMCONNECT_DATATYPE_INITPOSITION);

    const bool eventsDefined =
        SubscribeSystemEvent(EventSimStart, "SimStart") &&
        SubscribeSystemEvent(EventSimStop, "SimStop") &&
        SubscribeSystemEvent(EventObjectAdded, "ObjectAdded") &&
        SubscribeSystemEvent(EventObjectRemoved, "ObjectRemoved") &&
        MapClientEvent(EventFreezeLatitudeLongitude, "FREEZE_LATITUDE_LONGITUDE_SET") &&
        MapClientEvent(EventFreezeAltitude, "FREEZE_ALTITUDE_SET") &&
        MapClientEvent(EventFreezeAttitude, "FREEZE_ATTITUDE_SET") &&
        MapClientEvent(EventOpenAircraftDoors, "OPEN_AIRCRAFT_DOORS") &&
        MapClientEvent(EventCloseAircraftDoors, "CLOSE_AIRCRAFT_DOORS");
    return aircraftDefined && groundDefined && animationDefined && eventsDefined;
}

void SimConnectThread::BeginEnumerateObjects(
    SIMCONNECT_SIMOBJECT_TYPE type, std::shared_ptr<ISimConnectRequest> request)
{
    if (request->IsComplete()) {
        request->OnFailure();
        return;
    }
    const DWORD requestId = NextRequestId();
    const SendResult send = Capture(
        m_handle ? SimConnect_EnumerateSimObjectsAndLiveries(m_handle, requestId, type)
                 : E_HANDLE);
    TrackResponseRequest(send, requestId, request);
}

void SimConnectThread::BeginRequestObjectData(
    SIMCONNECT_DATA_DEFINITION_ID definition, DWORD objectId,
    SIMCONNECT_PERIOD period, DWORD interval,
    std::shared_ptr<ISimConnectRequest> request)
{
    if (request->IsComplete()) {
        request->OnFailure();
        return;
    }
    const DWORD requestId = NextRequestId();
    const SendResult send = Capture(
        m_handle && objectId != 0
            ? SimConnect_RequestDataOnSimObject(
                  m_handle, requestId, definition, objectId, period,
                  SIMCONNECT_DATA_REQUEST_FLAG_DEFAULT, 0, interval, 0)
            : E_HANDLE);
    TrackResponseRequest(send, requestId, request);
}

void SimConnectThread::BeginRequestObjectDataByType(
    SIMCONNECT_DATA_DEFINITION_ID definition, DWORD radiusMeters,
    SIMCONNECT_SIMOBJECT_TYPE type, std::shared_ptr<ISimConnectRequest> request)
{
    if (request->IsComplete()) {
        request->OnFailure();
        return;
    }
    const DWORD requestId = NextRequestId();
    const SendResult send = Capture(
        m_handle ? SimConnect_RequestDataOnSimObjectType(
                       m_handle, requestId, definition, radiusMeters, type)
                 : E_HANDLE);
    TrackResponseRequest(send, requestId, request);
}

void SimConnectThread::BeginCreateObject(
    std::string title, SIMCONNECT_DATA_INITPOSITION position,
    std::shared_ptr<ISimConnectRequest> request)
{
    if (request->IsComplete()) {
        request->OnFailure();
        return;
    }
    const DWORD requestId = NextRequestId();
    const SendResult send = Capture(
        m_handle && !title.empty()
            ? SimConnect_AICreateSimulatedObject_EX1(
                  m_handle, title.c_str(), "", position, requestId)
            : E_HANDLE);
    TrackResponseRequest(send, requestId, request);
}

void SimConnectThread::BeginRemoveObject(
    DWORD objectId, std::shared_ptr<ISimConnectRequest> request)
{
    if (request->IsComplete()) {
        request->OnFailure();
        return;
    }
    const DWORD requestId = NextRequestId();
    const SendResult send = Capture(
        m_handle && objectId != 0
            ? SimConnect_AIRemoveObject(m_handle, objectId, requestId)
            : E_HANDLE);
    CompleteCommand(send, requestId, request);
}

void SimConnectThread::BeginSetObjectData(
    SIMCONNECT_DATA_DEFINITION_ID definition, DWORD objectId, DWORD arrayCount,
    DWORD elementSize, const std::vector<std::uint8_t> &payload,
    std::shared_ptr<ISimConnectRequest> request)
{
    if (request->IsComplete()) {
        request->OnFailure();
        return;
    }
    const SendResult send = Capture(
        m_handle && objectId != 0 && elementSize != 0 && !payload.empty()
            ? SimConnect_SetDataOnSimObject(
                  m_handle, definition, objectId, 0, arrayCount, elementSize,
                  const_cast<void *>(static_cast<const void *>(payload.data())))
            : E_HANDLE);
    CompleteCommand(send, std::nullopt, request);
}

void SimConnectThread::BeginAddToDataDefinition(
    SIMCONNECT_DATA_DEFINITION_ID definition, std::string datumName,
    std::string units, SIMCONNECT_DATATYPE type,
    std::shared_ptr<ISimConnectRequest> request)
{
    if (request->IsComplete()) {
        request->OnFailure();
        return;
    }
    const SendResult send = Capture(
        m_handle && !datumName.empty()
            ? SimConnect_AddToDataDefinition(
                  m_handle, definition, datumName.c_str(),
                  units.empty() ? nullptr : units.c_str(), type)
            : E_HANDLE);
    CompleteCommand(send, std::nullopt, request);
}

void SimConnectThread::BeginTransmitEvent(
    DWORD objectId, SIMCONNECT_CLIENT_EVENT_ID eventId, DWORD data,
    std::shared_ptr<ISimConnectRequest> request)
{
    if (request->IsComplete()) {
        request->OnFailure();
        return;
    }
    const SendResult send = Capture(
        m_handle && objectId != 0
            ? SimConnect_TransmitClientEvent(
                  m_handle, objectId, eventId, data,
                  SIMCONNECT_GROUP_PRIORITY_HIGHEST,
                  SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY)
            : E_HANDLE);
    CompleteCommand(send, std::nullopt, request);
}

void SimConnectThread::BeginTransmitEventEx1(
    DWORD objectId, SIMCONNECT_CLIENT_EVENT_ID eventId, DWORD data0, DWORD data1,
    std::shared_ptr<ISimConnectRequest> request)
{
    if (request->IsComplete()) {
        request->OnFailure();
        return;
    }
    const SendResult send = Capture(
        m_handle && objectId != 0
            ? SimConnect_TransmitClientEvent_EX1(
                  m_handle, objectId, eventId,
                  SIMCONNECT_GROUP_PRIORITY_HIGHEST,
                  SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY, data0, data1)
            : E_HANDLE);
    CompleteCommand(send, std::nullopt, request);
}

bool SimConnectThread::TrackResponseRequest(
    const SendResult &send, DWORD requestId,
    const std::shared_ptr<ISimConnectRequest> &request)
{
    if (!send.Succeeded() || !send.sendId) {
        request->OnFailure();
        return false;
    }

    const auto [requestIt, requestInserted] =
        m_requestsByRequestId.emplace(requestId, request);
    if (!requestInserted) {
        request->OnFailure();
        return false;
    }
    if (!m_requestsBySendId.emplace(*send.sendId, request).second) {
        m_requestsByRequestId.erase(requestIt);
        request->OnFailure();
        return false;
    }
    return true;
}

void SimConnectThread::CompleteCommand(
    const SendResult &send, std::optional<DWORD> requestId,
    const std::shared_ptr<ISimConnectRequest> &request)
{
    if (!send.Succeeded() || !send.sendId) {
        request->OnFailure();
        return;
    }

    if (requestId &&
        !m_requestsByRequestId.emplace(*requestId, request).second) {
        request->OnFailure();
        return;
    }
    if (!m_requestsBySendId.emplace(*send.sendId, request).second) {
        if (requestId) m_requestsByRequestId.erase(*requestId);
        request->OnFailure();
        return;
    }

    request->OnMessage(nullptr, 0);
    if (request->IsComplete()) {
        RemoveRequest(request.get());
        request->OnSuccess();
    }
}

bool SimConnectThread::RouteRequestMessage(DWORD requestId,
                                           SIMCONNECT_RECV *message,
                                           DWORD messageSize)
{
    const auto found = m_requestsByRequestId.find(requestId);
    if (found == m_requestsByRequestId.end()) return false;

    const std::shared_ptr<ISimConnectRequest> request = found->second;
    request->OnMessage(message, messageSize);
    if (request->IsComplete()) {
        RemoveRequest(request.get());
        request->OnSuccess();
    }
    return true;
}

bool SimConnectThread::RouteRequestFailure(
    const SIMCONNECT_RECV_EXCEPTION &exception)
{
    const auto found = m_requestsBySendId.find(exception.dwSendID);
    if (found == m_requestsBySendId.end()) return false;

    const std::shared_ptr<ISimConnectRequest> request = found->second;
    RemoveRequest(request.get());
    request->OnFailure();
    return true;
}

void SimConnectThread::RemoveRequest(const ISimConnectRequest *request)
{
    std::erase_if(m_requestsByRequestId, [request](const auto &entry) {
        return entry.second.get() == request;
    });
    std::erase_if(m_requestsBySendId, [request](const auto &entry) {
        return entry.second.get() == request;
    });
}

void SimConnectThread::FailPendingRequests()
{
    std::unordered_map<ISimConnectRequest *, std::shared_ptr<ISimConnectRequest>> requests;
    for (const auto &[requestId, request] : m_requestsByRequestId) {
        static_cast<void>(requestId);
        requests.emplace(request.get(), request);
    }
    for (const auto &[sendId, request] : m_requestsBySendId) {
        static_cast<void>(sendId);
        requests.emplace(request.get(), request);
    }
    m_requestsByRequestId.clear();
    m_requestsBySendId.clear();
    for (const auto &[address, request] : requests) {
        static_cast<void>(address);
        request->OnFailure();
    }
}

void SimConnectThread::OnSimConnectMessage(SIMCONNECT_RECV *message,
                                           DWORD messageSize)
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
            NotifySimulation(true);
        } else if (event.uEventID == EventSimStop) {
            m_log("Simulation stopped.");
            NotifySimulation(false);
        }
        break;
    }
    case SIMCONNECT_RECV_ID_EVENT_OBJECT_ADDREMOVE: {
        const auto &event =
            *reinterpret_cast<SIMCONNECT_RECV_EVENT_OBJECT_ADDREMOVE *>(message);
        if (event.uEventID == EventObjectRemoved) NotifyObjectRemoved(event.dwData);
        break;
    }
    case SIMCONNECT_RECV_ID_ENUMERATE_SIMOBJECT_AND_LIVERY_LIST: {
        const auto &entry =
            *reinterpret_cast<SIMCONNECT_RECV_ENUMERATE_SIMOBJECT_AND_LIVERY_LIST *>(message);
        RouteRequestMessage(entry.dwRequestID, message, messageSize);
        break;
    }
    case SIMCONNECT_RECV_ID_SIMOBJECT_DATA_BYTYPE: {
        const auto &entry =
            *reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE *>(message);
        RouteRequestMessage(entry.dwRequestID, message, messageSize);
        break;
    }
    case SIMCONNECT_RECV_ID_SIMOBJECT_DATA: {
        const auto &entry =
            *reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA *>(message);
        RouteRequestMessage(entry.dwRequestID, message, messageSize);
        break;
    }
    case SIMCONNECT_RECV_ID_ASSIGNED_OBJECT_ID: {
        const auto &entry =
            *reinterpret_cast<SIMCONNECT_RECV_ASSIGNED_OBJECT_ID *>(message);
        RouteRequestMessage(entry.dwRequestID, message, messageSize);
        break;
    }
    case SIMCONNECT_RECV_ID_EXCEPTION:
        HandleException(*reinterpret_cast<SIMCONNECT_RECV_EXCEPTION *>(message));
        break;
    default:
        break;
    }
}

void SimConnectThread::HandleException(
    const SIMCONNECT_RECV_EXCEPTION &exception)
{
    std::ostringstream message;
    message << "SimConnect exception " << exception.dwException
            << " (sendId=" << exception.dwSendID;
    if (exception.dwIndex != SIMCONNECT_RECV_EXCEPTION::UNKNOWN_INDEX) {
        message << ", index=" << exception.dwIndex;
    }
    message << ')';
    if (!RouteRequestFailure(exception)) message << " for an untracked operation";
    m_log(message.str());
}

void SimConnectThread::NotifyObjectRemoved(DWORD objectId)
{
    for (ISimConnectStatus *observer : m_statusObservers) {
        observer->OnObjRemoved(objectId);
    }
}

void SimConnectThread::NotifyConnection(bool connected)
{
    for (ISimConnectStatus *observer : m_statusObservers) {
        if (connected) observer->OnSimConnected();
        else observer->OnSimDisconnected();
    }
}

void SimConnectThread::NotifySimulation(bool started)
{
    for (ISimConnectStatus *observer : m_statusObservers) {
        if (started) observer->OnSimStarted();
        else observer->OnSimStopped();
    }
}

void SimConnectThread::RegisterStatusObserver(ISimConnectStatus *observer)
{
    if (observer) m_statusObservers.push_back(observer);
}

SendResult SimConnectThread::Capture(HRESULT result) const
{
    SendResult captured{result};
    DWORD sendId = 0;
    if (m_handle && SUCCEEDED(result) &&
        SUCCEEDED(SimConnect_GetLastSentPacketID(m_handle, &sendId))) {
        captured.sendId = sendId;
    }
    return captured;
}

DWORD SimConnectThread::NextRequestId()
{
    return m_nextRequestId++;
}

bool SimConnectThread::AddDatum(SIMCONNECT_DATA_DEFINITION_ID definition,
                                const char *name, const char *units,
                                SIMCONNECT_DATATYPE type)
{
    if (!m_handle || FAILED(SimConnect_AddToDataDefinition(
                         m_handle, definition, name, units, type))) {
        m_log(std::string("Failed to define SimVar: ") + name);
        return false;
    }
    return true;
}

bool SimConnectThread::SubscribeSystemEvent(
    SIMCONNECT_CLIENT_EVENT_ID eventId, const char *name)
{
    if (!m_handle ||
        FAILED(SimConnect_SubscribeToSystemEvent(m_handle, eventId, name))) {
        m_log(std::string("Failed to subscribe to SimConnect event: ") + name);
        return false;
    }
    return true;
}

bool SimConnectThread::MapClientEvent(SIMCONNECT_CLIENT_EVENT_ID eventId,
                                      const char *name)
{
    if (!m_handle ||
        FAILED(SimConnect_MapClientEventToSimEvent(m_handle, eventId, name))) {
        m_log(std::string("Failed to map SimConnect event: ") + name);
        return false;
    }
    return true;
}

void CALLBACK SimConnectThread::DispatchThunk(SIMCONNECT_RECV *message,
                                              DWORD messageSize,
                                              void *context)
{
    auto &thread = *static_cast<SimConnectThread *>(context);
    thread.OnSimConnectMessage(message, messageSize);
}
} // namespace parking_services
