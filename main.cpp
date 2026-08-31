#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Windows.h>
#include <SimConnect.h>
#include <conio.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace
{
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr DWORD kScanRadiusMeters = 10'000;
constexpr auto kScanInterval = 2s;
constexpr auto kRequestTimeout = 8s;
constexpr auto kStableParkingTime = 10s;
constexpr auto kServiceCooldown = 5s;
constexpr double kParkedSpeedKnots = 1.0;
constexpr double kSafeServiceSpeedKnots = 2.0;
constexpr double kWorkerWalkingSpeedKnots = 2.5;
constexpr double kAnimationFramesPerSecond = 30.0;
constexpr auto kAnimationUpdateInterval = 33ms;
constexpr std::string_view kFsdtCateringTitle = "FSDT_Catering_EU";
constexpr std::string_view kBaggageCartTitle = "ASO_Baggage_Cart01";
constexpr std::string_view kFsdtWorkerTitle = "FSDT_catering_man_01";
constexpr std::string_view kFsdtWingwalkerTitle = "FSDT_Wingwalker_Male_04";
constexpr std::string_view kFsdtMarshallerTitle = "FSDT_Marshaller_01";
constexpr std::string_view kAsoboMarshallerTitle = "Marshaller_Male_Summer_Caucasian";

enum DefinitionId : SIMCONNECT_DATA_DEFINITION_ID
{
    DefinitionAircraft = 1,
    DefinitionGround,
    DefinitionWaypoint,
    DefinitionAnimationProbe,
    DefinitionAnimationDriver,
    DefinitionDirectPosition,
};

enum RequestId : SIMCONNECT_DATA_REQUEST_ID
{
    RequestAircraftSnapshot = 1,
    RequestGroundSnapshot,
    RequestCatalogAll,
    RequestCatalogAircraft,
    RequestCatalogHelicopter,
    RequestCatalogBoat,
    RequestCatalogGround,
    RequestCatalogBalloon,
    RequestCatalogAnimal,
    RequestAnimationProbe,
};

enum EventId : SIMCONNECT_CLIENT_EVENT_ID
{
    EventSimStart = 1,
    EventSimStop,
    EventObjectAdded,
    EventObjectRemoved,
    EventRequestCatering,
    EventRequestPower,
    EventRequestBaggage,
    EventFreezeLatitudeLongitude,
    EventFreezeAltitude,
    EventFreezeAttitude,
};

#pragma pack(push, 1)
struct AircraftData
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
    std::int32_t onGround{};
    std::int32_t isUser{};
    std::array<char, 256> currentAirport{};
    std::array<char, 256> assignedParking{};
    std::array<char, 256> fromAirport{};
    std::array<char, 256> toAirport{};
    std::int32_t etdSeconds{};
    std::int32_t etaSeconds{};
    std::array<char, 256> trafficState{};
};

struct GroundData
{
    std::array<char, 256> title{};
    double latitude{};
    double longitude{};
    double groundSpeedKnots{};
};

struct AnimationProbeData
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

struct DirectPositionData
{
    double latitude{};
    double longitude{};
    double altitudeFeet{};
    double headingDegrees{};
};
#pragma pack(pop)

static_assert(sizeof(AircraftData) == 2368, "Aircraft data definition layout changed");
static_assert(sizeof(GroundData) == 280, "Ground data definition layout changed");
static_assert(sizeof(AnimationProbeData) == 324, "Animation probe data definition layout changed");
static_assert(sizeof(DirectPositionData) == 32, "Direct position data definition layout changed");

struct AircraftRecord
{
    AircraftData data{};
    Clock::time_point firstSeen{};
    Clock::time_point lastSeen{};
    std::optional<Clock::time_point> parkedSince;
};

struct GroundRecord
{
    GroundData data{};
    Clock::time_point lastSeen{};
};

struct SentService
{
    std::string name;
    DWORD objectId{};
};

struct PendingCreate
{
    DWORD aircraftObjectId{};
    std::string title;
};

struct AnimatedWorker
{
    std::string title;
    Clock::time_point started{};
    Clock::time_point nextUpdate{};
    std::array<SIMCONNECT_DATA_INITPOSITION, 4> route{};
};

using CatalogEntry = std::pair<std::string, std::string>;

struct CatalogBucket
{
    std::string label;
    SIMCONNECT_SIMOBJECT_TYPE simObjectType{};
    bool excluded{};
    bool complete{};
    std::set<CatalogEntry> entries;
};

template <std::size_t Size> std::string FixedString(const std::array<char, Size> &value)
{
    const auto end = std::find(value.begin(), value.end(), '\0');
    return {value.data(), static_cast<std::size_t>(end - value.begin())};
}

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string NormalizedState(const AircraftData &data)
{
    std::string state = Lower(FixedString(data.trafficState));
    std::replace(state.begin(), state.end(), '_', ' ');
    if (state.starts_with("state ")) {
        state.erase(0, 6);
    }
    return state;
}

bool HasRoute(const AircraftData &data)
{
    return !FixedString(data.fromAirport).empty() && !FixedString(data.toAirport).empty();
}

bool IsInstantlyParked(const AircraftData &data)
{
    return data.isUser == 0 && data.onGround != 0 && std::abs(data.groundSpeedKnots) < kParkedSpeedKnots;
}

std::string Phase(const AircraftData &data)
{
    const std::string state = NormalizedState(data);
    const std::string current = Lower(FixedString(data.currentAirport));
    const std::string origin = Lower(FixedString(data.fromAirport));
    const std::string destination = Lower(FixedString(data.toAirport));

    if (state == "shutdown" || state == "postflight support" ||
        (!destination.empty() && current == destination && current != origin)) {
        return "arrival";
    }
    if (state == "flt plan" || state == "startup" || state == "preflight support" || state == "clearance" ||
        (!origin.empty() && current == origin && current != destination)) {
        return "departure";
    }
    if (state.find("push back") != std::string::npos || state.find("taxi") != std::string::npos) {
        return "moving";
    }
    if ((state == "sleep" || state.empty()) && !HasRoute(data)) {
        return "static/unknown";
    }
    return "ambiguous";
}

double DistanceMeters(double latitudeA, double longitudeA, double latitudeB, double longitudeB)
{
    constexpr double earthRadiusMeters = 6'371'000.0;
    constexpr double degreesToRadians = 3.14159265358979323846 / 180.0;
    const double lat1 = latitudeA * degreesToRadians;
    const double lat2 = latitudeB * degreesToRadians;
    const double deltaLat = (latitudeB - latitudeA) * degreesToRadians;
    const double deltaLon = (longitudeB - longitudeA) * degreesToRadians;
    const double haversine = std::sin(deltaLat / 2.0) * std::sin(deltaLat / 2.0) +
                             std::cos(lat1) * std::cos(lat2) * std::sin(deltaLon / 2.0) *
                                 std::sin(deltaLon / 2.0);
    return 2.0 * earthRadiusMeters * std::asin(std::sqrt((std::min)(haversine, 1.0)));
}

template <typename Payload, typename Entry>
std::optional<Payload> ReadPayload(const Entry &entry, DWORD callbackSize)
{
    const auto *entryBytes = reinterpret_cast<const BYTE *>(&entry);
    const auto *payloadBytes = reinterpret_cast<const BYTE *>(&entry.dwData);
    const std::size_t payloadOffset = static_cast<std::size_t>(payloadBytes - entryBytes);
    const std::size_t receivedSize = entry.dwSize != 0
                                         ? (std::min)(static_cast<std::size_t>(entry.dwSize),
                                                      static_cast<std::size_t>(callbackSize))
                                         : static_cast<std::size_t>(callbackSize);
    if (receivedSize < payloadOffset + sizeof(Payload)) {
        return std::nullopt;
    }

    Payload payload{};
    std::memcpy(&payload, payloadBytes, sizeof(payload));
    return payload;
}

class ProbeApp final
{
  public:
    int Run()
    {
        std::cout << "ParkingServices - MSFS 2024 ground-service probe\n"
                  << "Scans " << kScanRadiusMeters / 1000 << " km around the user aircraft.\n\n";
        PrintHelp();
        PrintPrompt();

        auto nextConnectionAttempt = Clock::now();
        while (!m_quit) {
            const auto now = Clock::now();
            if (!m_simConnect && now >= nextConnectionAttempt) {
                if (!Connect()) {
                    nextConnectionAttempt = now + 2s;
                }
            }

            if (m_simConnect) {
                const HRESULT dispatchResult = SimConnect_CallDispatch(m_simConnect, DispatchThunk, this);
                if (m_disconnectRequested) {
                    Disconnect();
                    nextConnectionAttempt = now + 2s;
                } else if (FAILED(dispatchResult)) {
                    LogLine("SimConnect dispatch failed; reconnecting.");
                    Disconnect();
                    nextConnectionAttempt = now + 2s;
                } else {
                    MaintainRequests(now);
                }
            }

            PumpConsoleInput();
            Sleep(10);
        }

        Disconnect();
        std::cout << "\nProbe stopped.\n";
        return 0;
    }

  private:
    static void CALLBACK DispatchThunk(SIMCONNECT_RECV *received, DWORD callbackSize, void *context)
    {
        static_cast<ProbeApp *>(context)->Dispatch(received, callbackSize);
    }

    bool Connect()
    {
        HANDLE connection = nullptr;
        const HRESULT result = SimConnect_Open(&connection, "ParkingServices Probe", nullptr, 0, nullptr, 0);
        if (FAILED(result)) {
            if (!m_reportedWaiting) {
                LogLine("Waiting for Microsoft Flight Simulator 2024...");
                m_reportedWaiting = true;
            }
            return false;
        }

        m_simConnect = connection;
        m_disconnectRequested = false;
        m_reportedWaiting = false;
        if (!DefineData() || !DefineEvents()) {
            LogLine("Failed to initialize SimConnect definitions; reconnecting.");
            Disconnect();
            return false;
        }

        LogLine("Connected to MSFS 2024.");
        RequestSnapshots(true);
        return true;
    }

    void Disconnect()
    {
        if (m_animationProbeActive) {
            StopAnimationProbe(true);
        }
        if (m_simConnect) {
            SimConnect_Close(m_simConnect);
            m_simConnect = nullptr;
        }
        m_aircraftRequestPending = false;
        m_groundRequestPending = false;
        m_disconnectRequested = false;
        m_selectedObjectId.reset();
        m_aircraft.clear();
        m_ground.clear();
        m_pendingAircraft.clear();
        m_pendingGround.clear();
        m_sentPackets.clear();
        m_createPackets.clear();
        m_waypointPackets.clear();
        m_animatedWorkers.clear();
        m_pendingCreates.clear();
        m_createdObjects.clear();
        m_catalogPackets.clear();
        m_catalogs.clear();
        m_catalogInProgress = false;
    }

    bool DefineData()
    {
        return AddDatum(DefinitionAircraft, "TITLE", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
               AddDatum(DefinitionAircraft, "ATC ID", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
               AddDatum(DefinitionAircraft, "ATC AIRLINE", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
               AddDatum(DefinitionAircraft, "ATC FLIGHT NUMBER", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
               AddDatum(DefinitionAircraft, "PLANE LATITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionAircraft, "PLANE LONGITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionAircraft, "PLANE ALTITUDE", "feet", SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionAircraft, "GROUND ALTITUDE", "feet", SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionAircraft, "PLANE HEADING DEGREES TRUE", "degrees",
                        SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionAircraft, "GROUND VELOCITY", "knots", SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionAircraft, "SIM ON GROUND", "bool", SIMCONNECT_DATATYPE_INT32) &&
               AddDatum(DefinitionAircraft, "IS USER SIM", "bool", SIMCONNECT_DATATYPE_INT32) &&
               AddDatum(DefinitionAircraft, "AI TRAFFIC CURRENT AIRPORT", nullptr,
                        SIMCONNECT_DATATYPE_STRING256) &&
               AddDatum(DefinitionAircraft, "AI TRAFFIC ASSIGNED PARKING", nullptr,
                        SIMCONNECT_DATATYPE_STRING256) &&
               AddDatum(DefinitionAircraft, "AI TRAFFIC FROMAIRPORT", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
               AddDatum(DefinitionAircraft, "AI TRAFFIC TOAIRPORT", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
               AddDatum(DefinitionAircraft, "AI TRAFFIC ETD", "seconds", SIMCONNECT_DATATYPE_INT32) &&
               AddDatum(DefinitionAircraft, "AI TRAFFIC ETA", "seconds", SIMCONNECT_DATATYPE_INT32) &&
               AddDatum(DefinitionAircraft, "AI TRAFFIC STATE", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
               AddDatum(DefinitionGround, "TITLE", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
               AddDatum(DefinitionGround, "PLANE LATITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionGround, "PLANE LONGITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionGround, "GROUND VELOCITY", "knots", SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionWaypoint, "AI Waypoint List", "number", SIMCONNECT_DATATYPE_WAYPOINT) &&
               AddDatum(DefinitionAnimationProbe, "TITLE", nullptr, SIMCONNECT_DATATYPE_STRING256) &&
               AddDatum(DefinitionAnimationProbe, "GROUND VELOCITY", "knots", SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionAnimationProbe, "VELOCITY BODY X", "meters per second",
                        SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionAnimationProbe, "VELOCITY BODY Y", "meters per second",
                        SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionAnimationProbe, "VELOCITY BODY Z", "meters per second",
                        SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionAnimationProbe, "PLANE HEADING DEGREES TRUE", "degrees",
                        SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionAnimationProbe, "PLANE LATITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionAnimationProbe, "PLANE LONGITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionAnimationProbe, "PLANE ALTITUDE", "feet", SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionAnimationProbe, "SIM ON GROUND", "bool", SIMCONNECT_DATATYPE_INT32) &&
               AddDatum(DefinitionAnimationDriver, "VELOCITY BODY Y", "meters per second",
                         SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionDirectPosition, "PLANE LATITUDE", "degrees",
                         SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionDirectPosition, "PLANE LONGITUDE", "degrees",
                         SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionDirectPosition, "PLANE ALTITUDE", "feet",
                         SIMCONNECT_DATATYPE_FLOAT64) &&
               AddDatum(DefinitionDirectPosition, "PLANE HEADING DEGREES TRUE", "degrees",
                         SIMCONNECT_DATATYPE_FLOAT64);
    }

    bool AddDatum(SIMCONNECT_DATA_DEFINITION_ID definition, const char *name, const char *units,
                  SIMCONNECT_DATATYPE type)
    {
        const HRESULT result = SimConnect_AddToDataDefinition(m_simConnect, definition, name, units, type);
        if (FAILED(result)) {
            LogLine(std::string("Failed to define SimVar: ") + name);
            return false;
        }
        return true;
    }

    bool DefineEvents()
    {
        return Check(SimConnect_SubscribeToSystemEvent(m_simConnect, EventSimStart, "SimStart"), "SimStart") &&
               Check(SimConnect_SubscribeToSystemEvent(m_simConnect, EventSimStop, "SimStop"), "SimStop") &&
               Check(SimConnect_SubscribeToSystemEvent(m_simConnect, EventObjectAdded, "ObjectAdded"),
                     "ObjectAdded") &&
               Check(SimConnect_SubscribeToSystemEvent(m_simConnect, EventObjectRemoved, "ObjectRemoved"),
                     "ObjectRemoved") &&
               Check(SimConnect_MapClientEventToSimEvent(m_simConnect, EventRequestCatering, "REQUEST_CATERING"),
                     "REQUEST_CATERING") &&
               Check(SimConnect_MapClientEventToSimEvent(m_simConnect, EventRequestPower, "REQUEST_POWER_SUPPLY"),
                     "REQUEST_POWER_SUPPLY") &&
               Check(SimConnect_MapClientEventToSimEvent(m_simConnect, EventRequestBaggage, "REQUEST_LUGGAGE"),
                      "REQUEST_LUGGAGE") &&
               Check(SimConnect_MapClientEventToSimEvent(
                         m_simConnect, EventFreezeLatitudeLongitude, "FREEZE_LATITUDE_LONGITUDE_SET"),
                     "FREEZE_LATITUDE_LONGITUDE_SET") &&
               Check(SimConnect_MapClientEventToSimEvent(m_simConnect, EventFreezeAltitude,
                                                         "FREEZE_ALTITUDE_SET"),
                     "FREEZE_ALTITUDE_SET") &&
               Check(SimConnect_MapClientEventToSimEvent(m_simConnect, EventFreezeAttitude,
                                                         "FREEZE_ATTITUDE_SET"),
                     "FREEZE_ATTITUDE_SET");
    }

    bool Check(HRESULT result, std::string_view operation)
    {
        if (FAILED(result)) {
            LogLine("SimConnect setup failed for " + std::string(operation) + ".");
            return false;
        }
        return true;
    }

    void Dispatch(SIMCONNECT_RECV *received, DWORD callbackSize)
    {
        if (!received) {
            return;
        }

        switch (received->dwID) {
        case SIMCONNECT_RECV_ID_OPEN:
            break;
        case SIMCONNECT_RECV_ID_QUIT:
            LogLine("MSFS closed the SimConnect connection.");
            m_disconnectRequested = true;
            break;
        case SIMCONNECT_RECV_ID_EVENT:
            HandleEvent(*reinterpret_cast<SIMCONNECT_RECV_EVENT *>(received));
            break;
        case SIMCONNECT_RECV_ID_EVENT_OBJECT_ADDREMOVE:
            HandleObjectEvent(*reinterpret_cast<SIMCONNECT_RECV_EVENT_OBJECT_ADDREMOVE *>(received));
            break;
        case SIMCONNECT_RECV_ID_SIMOBJECT_DATA_BYTYPE:
            HandleObjectData(*reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE *>(received), callbackSize);
            break;
        case SIMCONNECT_RECV_ID_SIMOBJECT_DATA:
            HandleAnimationProbeData(*reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA *>(received), callbackSize);
            break;
        case SIMCONNECT_RECV_ID_ASSIGNED_OBJECT_ID:
            HandleAssignedObject(*reinterpret_cast<SIMCONNECT_RECV_ASSIGNED_OBJECT_ID *>(received));
            break;
        case SIMCONNECT_RECV_ID_ENUMERATE_SIMOBJECT_AND_LIVERY_LIST:
            HandleCatalogData(*reinterpret_cast<SIMCONNECT_RECV_ENUMERATE_SIMOBJECT_AND_LIVERY_LIST *>(received),
                              callbackSize);
            break;
        case SIMCONNECT_RECV_ID_EXCEPTION:
            HandleException(*reinterpret_cast<SIMCONNECT_RECV_EXCEPTION *>(received));
            break;
        default:
            break;
        }
    }

    void HandleEvent(const SIMCONNECT_RECV_EVENT &event)
    {
        if (event.uEventID == EventSimStart) {
            LogLine("Simulation started.");
            RequestSnapshots(true);
        } else if (event.uEventID == EventSimStop) {
            LogLine("Simulation stopped.");
        }
    }

    void HandleObjectEvent(const SIMCONNECT_RECV_EVENT_OBJECT_ADDREMOVE &event)
    {
        const DWORD objectId = event.dwData;
        if (event.uEventID == EventObjectRemoved) {
            m_aircraft.erase(objectId);
            m_ground.erase(objectId);
            m_createdObjects.erase(objectId);
            m_animatedWorkers.erase(objectId);
            if (m_selectedObjectId == objectId) {
                m_selectedObjectId.reset();
                LogLine("Selected aircraft was removed from the simulation.");
            }
            if (m_animationProbeActive && m_animationProbeObjectId == objectId) {
                StopAnimationProbe(true);
                LogLine("The animation-probe object was removed from the simulation.");
            }
        }

        if (event.eObjType == SIMCONNECT_SIMOBJECT_TYPE_AIRCRAFT ||
            event.eObjType == SIMCONNECT_SIMOBJECT_TYPE_GROUND) {
            RequestSnapshots(false);
        }
    }

    void HandleObjectData(const SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry, DWORD callbackSize)
    {
        if (entry.dwRequestID == RequestAircraftSnapshot) {
            const auto payload = ReadPayload<AircraftData>(entry, callbackSize);
            if (!payload) {
                LogLine("Ignored a truncated aircraft data packet.");
                return;
            }
            if (entry.dwentrynumber == 1) {
                m_pendingAircraft.clear();
            }
            m_pendingAircraft[entry.dwObjectID] = *payload;
            if (entry.dwentrynumber == entry.dwoutof) {
                CommitAircraftSnapshot();
            }
        } else if (entry.dwRequestID == RequestGroundSnapshot) {
            const auto payload = ReadPayload<GroundData>(entry, callbackSize);
            if (!payload) {
                LogLine("Ignored a truncated ground-object data packet.");
                return;
            }
            if (entry.dwentrynumber == 1) {
                m_pendingGround.clear();
            }
            m_pendingGround[entry.dwObjectID] = *payload;
            if (entry.dwentrynumber == entry.dwoutof) {
                CommitGroundSnapshot();
            }
        }
    }

    void HandleAnimationProbeData(const SIMCONNECT_RECV_SIMOBJECT_DATA &entry, DWORD callbackSize)
    {
        if (!m_animationProbeActive || entry.dwRequestID != RequestAnimationProbe ||
            entry.dwObjectID != m_animationProbeObjectId || !m_animationProbeFile) {
            return;
        }

        const auto payload = ReadPayload<AnimationProbeData>(entry, callbackSize);
        if (!payload) {
            LogLine("Ignored a truncated animation-probe data packet.");
            return;
        }
        m_animationProbeRequestPacket.reset();

        const double elapsedSeconds =
            std::chrono::duration<double>(Clock::now() - m_animationProbeStarted).count();
        std::string title = FixedString(payload->title);
        std::size_t quote = 0;
        while ((quote = title.find('"', quote)) != std::string::npos) {
            title.insert(quote, 1, '"');
            quote += 2;
        }

        m_animationProbeFile << std::fixed << std::setprecision(6) << elapsedSeconds << ','
                             << entry.dwObjectID << ",\"" << title << "\"," << payload->groundSpeedKnots
                             << ',' << payload->velocityBodyXMetersPerSecond << ','
                             << payload->velocityBodyYMetersPerSecond << ','
                             << payload->velocityBodyZMetersPerSecond << ',' << payload->headingDegrees << ','
                             << payload->latitude << ',' << payload->longitude << ',' << payload->altitudeFeet
                             << ',' << payload->onGround << '\n';
        ++m_animationProbeSamples;
        if (m_animationProbeSamples % 20 == 0) {
            m_animationProbeFile.flush();
        }
    }

    void HandleCatalogData(const SIMCONNECT_RECV_ENUMERATE_SIMOBJECT_AND_LIVERY_LIST &message, DWORD callbackSize)
    {
        const auto bucketIt = m_catalogs.find(message.dwRequestID);
        if (!m_catalogInProgress || bucketIt == m_catalogs.end()) {
            return;
        }

        const auto *messageBytes = reinterpret_cast<const BYTE *>(&message);
        const auto *entriesBytes = reinterpret_cast<const BYTE *>(&message.rgData);
        const std::size_t entriesOffset = static_cast<std::size_t>(entriesBytes - messageBytes);
        const std::size_t receivedSize = message.dwSize != 0
                                             ? (std::min)(static_cast<std::size_t>(message.dwSize),
                                                          static_cast<std::size_t>(callbackSize))
                                             : static_cast<std::size_t>(callbackSize);
        const std::size_t availableEntries = receivedSize > entriesOffset
                                                 ? (receivedSize - entriesOffset) /
                                                       sizeof(SIMCONNECT_ENUMERATE_SIMOBJECT_LIVERY)
                                                 : 0;
        const std::size_t entryCount =
            (std::min)(static_cast<std::size_t>(message.dwArraySize), availableEntries);
        auto &bucket = bucketIt->second;
        for (std::size_t index = 0; index < entryCount; ++index) {
            const auto &entry = message.rgData[index];
            bucket.entries.emplace(std::string(entry.AircraftTitle), std::string(entry.LiveryName));
        }

        if (entryCount != message.dwArraySize) {
            LogLine("The SimObject catalog contained a truncated response page.");
        }

        const bool complete = message.dwOutOf == 0 || message.dwEntryNumber + 1 >= message.dwOutOf;
        if (complete) {
            bucket.complete = true;
            FinishCatalogIfComplete();
        }
    }

    void HandleAssignedObject(const SIMCONNECT_RECV_ASSIGNED_OBJECT_ID &message)
    {
        const auto pending = m_pendingCreates.find(message.dwRequestID);
        if (pending == m_pendingCreates.end()) {
            return;
        }

        const PendingCreate created = pending->second;
        m_createdObjects.insert(message.dwObjectID);
        LogLine("Created " + created.title + " for aircraft " +
                std::to_string(created.aircraftObjectId) + " as ObjectID " +
                std::to_string(message.dwObjectID) + ".");
        m_pendingCreates.erase(pending);

        std::erase_if(m_createPackets, [&message](const auto &item) {
            return item.second == message.dwRequestID;
        });

        if (created.title == kAsoboMarshallerTitle) {
            AssignWalkingLoop(message.dwObjectID, created);
        }
        if (created.title == kFsdtWingwalkerTitle || created.title == kFsdtMarshallerTitle) {
            StartDirectWalkingLoop(message.dwObjectID, created);
        }
    }

    void CommitAircraftSnapshot()
    {
        const auto now = Clock::now();
        std::map<DWORD, AircraftRecord> updated;
        for (const auto &[objectId, data] : m_pendingAircraft) {
            AircraftRecord record{};
            record.data = data;
            record.firstSeen = now;
            record.lastSeen = now;

            const auto previous = m_aircraft.find(objectId);
            if (previous != m_aircraft.end()) {
                record.firstSeen = previous->second.firstSeen;
                record.parkedSince = previous->second.parkedSince;
            }

            if (IsInstantlyParked(data)) {
                if (!record.parkedSince) {
                    record.parkedSince = now;
                }
            } else {
                record.parkedSince.reset();
            }
            updated.emplace(objectId, std::move(record));
        }

        m_aircraft = std::move(updated);
        m_pendingAircraft.clear();
        m_aircraftRequestPending = false;
        if (m_selectedObjectId && !m_aircraft.contains(*m_selectedObjectId)) {
            m_selectedObjectId.reset();
            LogLine("Selected aircraft is no longer in the scan results.");
        }
    }

    void CommitGroundSnapshot()
    {
        const auto now = Clock::now();
        std::map<DWORD, GroundRecord> updated;
        for (const auto &[objectId, data] : m_pendingGround) {
            updated.emplace(objectId, GroundRecord{data, now});
        }
        m_ground = std::move(updated);
        m_pendingGround.clear();
        m_groundRequestPending = false;
    }

    void HandleException(const SIMCONNECT_RECV_EXCEPTION &exception)
    {
        std::ostringstream message;
        message << "SimConnect exception " << exception.dwException << " (sendId=" << exception.dwSendID;
        if (exception.dwIndex != SIMCONNECT_RECV_EXCEPTION::UNKNOWN_INDEX) {
            message << ", index=" << exception.dwIndex;
        }
        message << ')';

        const auto sent = m_sentPackets.find(exception.dwSendID);
        if (sent != m_sentPackets.end()) {
            message << " while requesting " << sent->second.name << " for aircraft " << sent->second.objectId;
            m_sentPackets.erase(sent);
        }
        const auto catalogPacket = m_catalogPackets.find(exception.dwSendID);
        if (catalogPacket != m_catalogPackets.end()) {
            const auto bucket = m_catalogs.find(catalogPacket->second);
            if (bucket != m_catalogs.end()) {
                message << " while enumerating " << bucket->second.label;
                bucket->second.complete = true;
            }
            m_catalogPackets.erase(catalogPacket);
        }
        const auto createPacket = m_createPackets.find(exception.dwSendID);
        if (createPacket != m_createPackets.end()) {
            const auto pending = m_pendingCreates.find(createPacket->second);
            if (pending != m_pendingCreates.end()) {
                message << " while creating " << pending->second.title << " for aircraft "
                        << pending->second.aircraftObjectId;
                m_pendingCreates.erase(pending);
            }
            m_createPackets.erase(createPacket);
        }
        const auto waypointPacket = m_waypointPackets.find(exception.dwSendID);
        if (waypointPacket != m_waypointPackets.end()) {
            message << " while assigning walking loop to " << waypointPacket->second;
            m_waypointPackets.erase(waypointPacket);
        }
        if (m_animationProbeRequestPacket && *m_animationProbeRequestPacket == exception.dwSendID) {
            message << " while starting animation probe for ObjectID " << m_animationProbeObjectId;
            m_animationProbeRequestPacket.reset();
            StopAnimationProbe(false);
        }
        LogLine(message.str());
        FinishCatalogIfComplete();
    }

    void MaintainRequests(Clock::time_point now)
    {
        if (m_aircraftRequestPending && now - m_aircraftRequestStarted > kRequestTimeout) {
            m_aircraftRequestPending = false;
            m_pendingAircraft.clear();
        }
        if (m_groundRequestPending && now - m_groundRequestStarted > kRequestTimeout) {
            m_groundRequestPending = false;
            m_pendingGround.clear();
        }
        if (now >= m_nextScan) {
            RequestSnapshots(false);
            m_nextScan = now + kScanInterval;
        }
        MaintainAnimatedWorkers(now);
    }

    void MaintainAnimatedWorkers(Clock::time_point now)
    {
        for (auto worker = m_animatedWorkers.begin(); worker != m_animatedWorkers.end();) {
            if (now < worker->second.nextUpdate) {
                ++worker;
                continue;
            }

            double transitionStart = 0.0;
            double transitionEnd = 0.0;
            double loopStart = 0.0;
            double loopEnd = 0.0;
            if (worker->second.title == kFsdtMarshallerTitle) {
                transitionStart = 1445.0;
                transitionEnd = 1530.0;
                loopStart = 1530.0;
                loopEnd = 1572.0;
            } else {
                transitionStart = 192.0;
                transitionEnd = 229.0;
                loopStart = 230.0;
                loopEnd = 268.0;
            }

            const double elapsedSeconds =
                std::chrono::duration<double>(now - worker->second.started).count();

            std::array<double, 4> segmentLengths{};
            double routeLength = 0.0;
            for (std::size_t index = 0; index < worker->second.route.size(); ++index) {
                const auto &from = worker->second.route[index];
                const auto &to = worker->second.route[(index + 1) % worker->second.route.size()];
                segmentLengths[index] =
                    DistanceMeters(from.Latitude, from.Longitude, to.Latitude, to.Longitude);
                routeLength += segmentLengths[index];
            }

            double routeDistance =
                std::fmod(elapsedSeconds * kWorkerWalkingSpeedKnots * 0.514444, routeLength);
            std::size_t segment = 0;
            while (segment + 1 < segmentLengths.size() && routeDistance > segmentLengths[segment]) {
                routeDistance -= segmentLengths[segment];
                ++segment;
            }

            const auto &from = worker->second.route[segment];
            const auto &to = worker->second.route[(segment + 1) % worker->second.route.size()];
            const double fraction = segmentLengths[segment] > 0.0
                                        ? routeDistance / segmentLengths[segment]
                                        : 0.0;
            SIMCONNECT_DATA_INITPOSITION position = from;
            position.Latitude = from.Latitude + (to.Latitude - from.Latitude) * fraction;
            position.Longitude = from.Longitude + (to.Longitude - from.Longitude) * fraction;
            position.Altitude = from.Altitude + (to.Altitude - from.Altitude) * fraction;
            const double meanLatitudeRadians =
                (from.Latitude + to.Latitude) * 0.5 * 3.14159265358979323846 / 180.0;
            const double north = to.Latitude - from.Latitude;
            const double east = (to.Longitude - from.Longitude) * std::cos(meanLatitudeRadians);
            position.Heading = std::fmod(std::atan2(east, north) * 180.0 /
                                             3.14159265358979323846 +
                                         360.0,
                                         360.0);
            position.Pitch = 0.0;
            position.Bank = 0.0;
            position.OnGround = 1;
            position.Airspeed = 0;

            const double transitionDuration =
                (transitionEnd - transitionStart) / kAnimationFramesPerSecond;
            double animationFrame = 0.0;
            if (elapsedSeconds < transitionDuration) {
                animationFrame = transitionStart + elapsedSeconds * kAnimationFramesPerSecond;
            } else {
                const double walkingFrames =
                    (elapsedSeconds - transitionDuration) * kAnimationFramesPerSecond;
                animationFrame = loopStart + std::fmod(walkingFrames, loopEnd - loopStart);
            }

            const HRESULT result = SimConnect_SetDataOnSimObject(
                m_simConnect, DefinitionAnimationDriver, worker->first, 0, 0,
                static_cast<DWORD>(sizeof(animationFrame)), &animationFrame);
            if (FAILED(result)) {
                LogLine("SimConnect immediately rejected the animation driver for " +
                        worker->second.title + " (ObjectID " + std::to_string(worker->first) + ").");
                worker = m_animatedWorkers.erase(worker);
                continue;
            }

            // Write only the changing world coordinates. Repeated Initial Position writes
            // retrigger MSFS ground placement and make human SimObjects jump vertically.
            DirectPositionData directPosition{
                position.Latitude, position.Longitude, position.Altitude, position.Heading};
            const HRESULT positionResult = SimConnect_SetDataOnSimObject(
                m_simConnect, DefinitionDirectPosition, worker->first, 0, 0,
                static_cast<DWORD>(sizeof(directPosition)), &directPosition);
            if (FAILED(positionResult)) {
                LogLine("SimConnect immediately rejected direct movement for " +
                        worker->second.title + " (ObjectID " + std::to_string(worker->first) + ").");
                worker = m_animatedWorkers.erase(worker);
                continue;
            }

            worker->second.nextUpdate = now + kAnimationUpdateInterval;
            ++worker;
        }
    }

    void StartDirectWalkingLoop(DWORD objectId, const PendingCreate &created)
    {
        const auto aircraft = m_aircraft.find(created.aircraftObjectId);
        if (aircraft == m_aircraft.end()) {
            LogLine("Could not start direct walking for " + created.title +
                    ": its aircraft is no longer in the scan.");
            return;
        }

        const std::array<SIMCONNECT_CLIENT_EVENT_ID, 3> freezeEvents{
            EventFreezeLatitudeLongitude, EventFreezeAltitude, EventFreezeAttitude};
        for (const auto eventId : freezeEvents) {
            const HRESULT result = SimConnect_TransmitClientEvent(
                m_simConnect, objectId, eventId, 1, SIMCONNECT_GROUP_PRIORITY_HIGHEST,
                SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
            if (FAILED(result)) {
                LogLine("Could not freeze physics for " + created.title + " (ObjectID " +
                        std::to_string(objectId) + ").");
                return;
            }
        }

        std::array<std::pair<double, double>, 4> offsets{};
        if (created.title == kFsdtWingwalkerTitle) {
            offsets = {{{-3.0, 19.0}, {-8.0, 19.0}, {-8.0, 25.0}, {-3.0, 25.0}}};
        } else {
            offsets = {{{0.0, 14.0}, {4.0, 14.0}, {4.0, 18.0}, {0.0, 18.0}}};
        }

        AnimatedWorker worker{};
        worker.title = created.title;
        worker.started = Clock::now();
        worker.nextUpdate = worker.started;
        for (std::size_t index = 0; index < offsets.size(); ++index) {
            worker.route[index] = OffsetPosition(aircraft->second.data, offsets[index].first,
                                                 offsets[index].second);
            worker.route[index].Altitude = aircraft->second.data.groundAltitudeFeet;
        }
        m_animatedWorkers.emplace(objectId, std::move(worker));
        LogLine("Froze physics and started GSX-style direct movement and walking animation for " +
                created.title + " (ObjectID " + std::to_string(objectId) + ").");
    }

    void RequestSnapshots(bool force)
    {
        if (!m_simConnect) {
            return;
        }

        const auto now = Clock::now();
        if (force) {
            m_aircraftRequestPending = false;
            m_groundRequestPending = false;
            m_pendingAircraft.clear();
            m_pendingGround.clear();
        }

        if (!m_aircraftRequestPending &&
            SUCCEEDED(SimConnect_RequestDataOnSimObjectType(m_simConnect, RequestAircraftSnapshot,
                                                            DefinitionAircraft, kScanRadiusMeters,
                                                            SIMCONNECT_SIMOBJECT_TYPE_AIRCRAFT))) {
            m_aircraftRequestPending = true;
            m_aircraftRequestStarted = now;
        }
        if (!m_groundRequestPending &&
            SUCCEEDED(SimConnect_RequestDataOnSimObjectType(m_simConnect, RequestGroundSnapshot, DefinitionGround,
                                                            kScanRadiusMeters,
                                                            SIMCONNECT_SIMOBJECT_TYPE_GROUND))) {
            m_groundRequestPending = true;
            m_groundRequestStarted = now;
        }
        m_nextScan = now + kScanInterval;
    }

    void PumpConsoleInput()
    {
        while (_kbhit()) {
            const int key = _getch();
            if (key == 0 || key == 224) {
                if (_kbhit()) {
                    static_cast<void>(_getch());
                }
                continue;
            }
            if (key == '\r') {
                std::cout << '\n';
                const std::string command = std::exchange(m_commandLine, {});
                HandleCommand(command);
                if (!m_quit) {
                    PrintPrompt();
                }
            } else if (key == '\b') {
                if (!m_commandLine.empty()) {
                    m_commandLine.pop_back();
                    std::cout << "\b \b" << std::flush;
                }
            } else if (key >= 32 && key <= 126) {
                m_commandLine.push_back(static_cast<char>(key));
                std::cout << static_cast<char>(key) << std::flush;
            }
        }
    }

    void HandleCommand(const std::string &line)
    {
        std::istringstream input(line);
        std::string command;
        input >> command;
        command = Lower(command);
        if (command.empty()) {
            return;
        }

        if (command == "help" || command == "?") {
            PrintHelp();
        } else if (command == "status") {
            PrintStatus();
        } else if (command == "aircraft" || command == "list") {
            PrintAircraft();
        } else if (command == "ground") {
            PrintGroundObjects();
        } else if (command == "select") {
            SelectAircraft(input);
        } else if (command == "target") {
            PrintTarget();
        } else if (command == "scan") {
            RequestSnapshots(true);
            std::cout << "Snapshot requested.\n";
        } else if (command == "catalog") {
            RequestCatalog();
        } else if (command == "catering") {
            RequestService(EventRequestCatering, "catering");
        } else if (command == "gpu" || command == "power") {
            RequestService(EventRequestPower, "ground power");
        } else if (command == "baggage" || command == "luggage") {
            RequestService(EventRequestBaggage, "baggage");
        } else if (command == "fsdt") {
            SpawnFsdtCatering();
        } else if (command == "clearfsdt") {
            ClearCreatedObjects();
        } else if (command == "animprobe") {
            StartAnimationProbe(input);
        } else if (command == "stopprobe") {
            StopAnimationProbe(true);
        } else if (command == "quit" || command == "exit") {
            m_quit = true;
        } else {
            std::cout << "Unknown command. Type 'help'.\n";
        }
    }

    void StartAnimationProbe(std::istringstream &input)
    {
        if (!m_simConnect) {
            std::cout << "Not connected to MSFS.\n";
            return;
        }

        std::uint64_t requestedObjectId = 0;
        if (!(input >> requestedObjectId) || requestedObjectId == 0 ||
            requestedObjectId > (std::numeric_limits<DWORD>::max)()) {
            std::cout << "Usage: animprobe <ObjectID>\n";
            return;
        }
        if (m_animationProbeActive) {
            StopAnimationProbe(true);
        }

        std::array<wchar_t, 32768> executablePathBuffer{};
        const DWORD pathLength = GetModuleFileNameW(nullptr, executablePathBuffer.data(),
                                                    static_cast<DWORD>(executablePathBuffer.size()));
        m_animationProbePath = "animation_probe.csv";
        if (pathLength != 0 && pathLength < executablePathBuffer.size()) {
            m_animationProbePath = std::filesystem::path(executablePathBuffer.data()).parent_path() /
                                   "animation_probe.csv";
        }

        m_animationProbeFile.open(m_animationProbePath, std::ios::trunc);
        if (!m_animationProbeFile) {
            std::cout << "Could not create " << m_animationProbePath.string() << ".\n";
            return;
        }
        m_animationProbeFile
            << "elapsed_seconds,object_id,title,ground_velocity_knots,velocity_body_x_mps,"
               "velocity_body_y_mps,velocity_body_z_mps,heading_degrees,latitude,longitude,"
               "altitude_feet,on_ground\n";
        m_animationProbeFile.flush();

        m_animationProbeObjectId = static_cast<DWORD>(requestedObjectId);
        m_animationProbeStarted = Clock::now();
        m_animationProbeSamples = 0;
        const HRESULT result = SimConnect_RequestDataOnSimObject(
            m_simConnect, RequestAnimationProbe, DefinitionAnimationProbe, m_animationProbeObjectId,
            SIMCONNECT_PERIOD_SIM_FRAME, SIMCONNECT_DATA_REQUEST_FLAG_DEFAULT, 0, 2, 0);
        if (FAILED(result)) {
            m_animationProbeFile.close();
            std::cout << "SimConnect immediately rejected the animation probe request.\n";
            return;
        }

        m_animationProbeActive = true;
        DWORD packetId = 0;
        if (SUCCEEDED(SimConnect_GetLastSentPacketID(m_simConnect, &packetId))) {
            m_animationProbeRequestPacket = packetId;
        }
        std::cout << "Recording ObjectID " << m_animationProbeObjectId << " until stopprobe. Output: "
                  << m_animationProbePath.string() << '\n';
    }

    void StopAnimationProbe(bool announce)
    {
        if (!m_animationProbeActive) {
            if (announce) {
                std::cout << "No animation probe is currently recording.\n";
            }
            return;
        }

        if (m_simConnect) {
            static_cast<void>(SimConnect_RequestDataOnSimObject(
                m_simConnect, RequestAnimationProbe, DefinitionAnimationProbe, m_animationProbeObjectId,
                SIMCONNECT_PERIOD_NEVER, SIMCONNECT_DATA_REQUEST_FLAG_DEFAULT, 0, 0, 0));
        }
        m_animationProbeFile.flush();
        m_animationProbeFile.close();
        m_animationProbeActive = false;
        m_animationProbeRequestPacket.reset();

        if (announce) {
            std::cout << "Stopped animation probe after " << m_animationProbeSamples
                      << " samples. Output: " << m_animationProbePath.string() << '\n';
        }
    }

    void RequestCatalog()
    {
        if (!m_simConnect) {
            std::cout << "Not connected to MSFS.\n";
            return;
        }
        if (m_catalogInProgress) {
            std::cout << "A SimObject catalog request is already in progress.\n";
            return;
        }

        m_catalogs = {
            {RequestCatalogAll, {"ALL", SIMCONNECT_SIMOBJECT_TYPE_ALL, false}},
            {RequestCatalogAircraft, {"AIRCRAFT", SIMCONNECT_SIMOBJECT_TYPE_AIRCRAFT, true}},
            {RequestCatalogHelicopter, {"HELICOPTER", SIMCONNECT_SIMOBJECT_TYPE_HELICOPTER, true}},
            {RequestCatalogBoat, {"BOAT", SIMCONNECT_SIMOBJECT_TYPE_BOAT, true}},
            {RequestCatalogGround, {"GROUND", SIMCONNECT_SIMOBJECT_TYPE_GROUND, false}},
            {RequestCatalogBalloon, {"HOT_AIR_BALLOON", SIMCONNECT_SIMOBJECT_TYPE_HOT_AIR_BALLOON, true}},
            {RequestCatalogAnimal, {"ANIMAL", SIMCONNECT_SIMOBJECT_TYPE_ANIMAL, true}},
        };
        m_catalogPackets.clear();
        m_catalogInProgress = true;

        for (auto &[requestId, bucket] : m_catalogs) {
            if (FAILED(SimConnect_EnumerateSimObjectsAndLiveries(m_simConnect, requestId,
                                                                 bucket.simObjectType))) {
                bucket.complete = true;
                std::cout << "MSFS rejected the " << bucket.label << " SimObject catalog request.\n";
            } else {
                DWORD packetId = 0;
                if (SUCCEEDED(SimConnect_GetLastSentPacketID(m_simConnect, &packetId))) {
                    m_catalogPackets[packetId] = requestId;
                }
            }
        }

        if (std::ranges::all_of(m_catalogs, [](const auto &item) { return item.second.complete; })) {
            m_catalogInProgress = false;
            return;
        }
        std::cout << "Requested categorized spawnable SimObject catalogs. Waiting for MSFS...\n";
    }

    void FinishCatalogIfComplete()
    {
        if (!m_catalogInProgress ||
            !std::ranges::all_of(m_catalogs, [](const auto &item) { return item.second.complete; })) {
            return;
        }
        m_catalogInProgress = false;

        std::array<wchar_t, 32768> executablePathBuffer{};
        const DWORD pathLength = GetModuleFileNameW(nullptr, executablePathBuffer.data(),
                                                    static_cast<DWORD>(executablePathBuffer.size()));
        std::filesystem::path outputPath = "simobject_catalog.txt";
        if (pathLength != 0 && pathLength < executablePathBuffer.size()) {
            outputPath = std::filesystem::path(executablePathBuffer.data()).parent_path() / outputPath;
        }

        std::ofstream output(outputPath, std::ios::trunc);
        if (!output) {
            LogLine("Could not create SimObject catalog file: " + outputPath.string());
            return;
        }

        const auto &allEntries = m_catalogs.at(RequestCatalogAll).entries;
        std::size_t retainedCount = 0;
        std::size_t excludedCount = 0;
        output << "ParkingServices filtered spawnable SimObject catalog\n"
               << "All entries: " << allEntries.size() << "\n"
               << "Excluded types: AIRCRAFT, HELICOPTER, HOT_AIR_BALLOON, BOAT, ANIMAL\n\n"
               << "[SPAWNABLE - FILTERED]\n";

        for (const auto &entry : allEntries) {
            bool excluded = false;
            std::string type = "UNKNOWN";
            for (const auto &[requestId, bucket] : m_catalogs) {
                if (requestId == RequestCatalogAll || !bucket.entries.contains(entry)) {
                    continue;
                }
                if (bucket.excluded) {
                    excluded = true;
                    break;
                }
                if (type == "UNKNOWN") {
                    type = bucket.label;
                }
            }

            if (excluded) {
                ++excludedCount;
            } else {
                output << "type=\"" << type << "\"\ttitle=\"" << entry.first
                       << "\"\tlivery=\"" << entry.second << "\"\n";
                ++retainedCount;
            }
        }
        output << "\nRetained entries: " << retainedCount << "\n"
               << "Excluded entries: " << excludedCount << "\n";
        output.close();

        LogLine("Wrote SimObject catalog to " + outputPath.string());
    }

    void SelectAircraft(std::istringstream &input)
    {
        DWORD objectId = 0;
        if (!(input >> objectId)) {
            std::cout << "Usage: select <ObjectID>\n";
            return;
        }

        const auto aircraft = m_aircraft.find(objectId);
        if (aircraft == m_aircraft.end()) {
            std::cout << "ObjectID " << objectId << " is not in the current aircraft scan.\n";
            return;
        }
        if (aircraft->second.data.isUser != 0) {
            std::cout << "The user aircraft cannot be selected by this probe.\n";
            return;
        }

        m_selectedObjectId = objectId;
        std::cout << "Selected aircraft " << objectId << ": " << FixedString(aircraft->second.data.title) << '\n';
        PrintTarget();
    }

    void RequestService(EventId eventId, std::string name)
    {
        if (!m_simConnect) {
            std::cout << "Not connected to MSFS.\n";
            return;
        }
        const auto now = Clock::now();
        std::size_t requested = 0;
        std::size_t skippedUnsafe = 0;
        std::size_t skippedCooldown = 0;
        std::size_t rejected = 0;

        for (const auto &[objectId, record] : m_aircraft) {
            const AircraftData &data = record.data;
            if (data.isUser != 0) {
                continue;
            }
            if (data.onGround == 0 || std::abs(data.groundSpeedKnots) >= kSafeServiceSpeedKnots) {
                ++skippedUnsafe;
                continue;
            }

            const std::uint64_t cooldownKey = (static_cast<std::uint64_t>(objectId) << 32U) |
                                              static_cast<std::uint32_t>(eventId);
            const auto previous = m_serviceRequests.find(cooldownKey);
            if (previous != m_serviceRequests.end() && now - previous->second < kServiceCooldown) {
                ++skippedCooldown;
                continue;
            }

            const HRESULT result = SimConnect_TransmitClientEvent(
                m_simConnect, objectId, eventId, 0, SIMCONNECT_GROUP_PRIORITY_HIGHEST,
                SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
            if (FAILED(result)) {
                ++rejected;
                continue;
            }

            m_serviceRequests[cooldownKey] = now;
            DWORD packetId = 0;
            if (SUCCEEDED(SimConnect_GetLastSentPacketID(m_simConnect, &packetId))) {
                m_sentPackets[packetId] = SentService{name, objectId};
            }
            ++requested;
        }

        std::cout << "Requested " << name << " for " << requested << " parked AI aircraft";
        if (skippedUnsafe != 0 || skippedCooldown != 0 || rejected != 0) {
            std::cout << " (skipped: " << skippedUnsafe << " moving/airborne, " << skippedCooldown
                      << " cooldown, " << rejected << " rejected)";
        }
        std::cout << ".\n";
        RequestSnapshots(false);
    }

    static SIMCONNECT_DATA_INITPOSITION OffsetPosition(const AircraftData &aircraft, double forwardMeters,
                                                       double rightMeters)
    {
        constexpr double earthRadiusMeters = 6'371'000.0;
        constexpr double degreesToRadians = 3.14159265358979323846 / 180.0;
        constexpr double radiansToDegrees = 180.0 / 3.14159265358979323846;
        const double heading = aircraft.headingDegrees * degreesToRadians;
        const double northMeters = forwardMeters * std::cos(heading) - rightMeters * std::sin(heading);
        const double eastMeters = forwardMeters * std::sin(heading) + rightMeters * std::cos(heading);
        const double latitudeRadians = aircraft.latitude * degreesToRadians;

        SIMCONNECT_DATA_INITPOSITION position{};
        position.Latitude = aircraft.latitude + northMeters / earthRadiusMeters * radiansToDegrees;
        const double longitudeScale = earthRadiusMeters * (std::max)(std::abs(std::cos(latitudeRadians)), 0.01);
        position.Longitude = aircraft.longitude + eastMeters / longitudeScale * radiansToDegrees;
        position.Altitude = aircraft.altitudeFeet;
        position.Pitch = 0.0;
        position.Bank = 0.0;
        position.Heading = aircraft.headingDegrees;
        position.OnGround = 1;
        position.Airspeed = 0;
        return position;
    }

    void AssignWalkingLoop(DWORD objectId, const PendingCreate &created)
    {
        const auto aircraft = m_aircraft.find(created.aircraftObjectId);
        if (aircraft == m_aircraft.end()) {
            LogLine("Could not assign a walking loop to " + created.title +
                    ": its aircraft is no longer in the scan.");
            return;
        }

        std::array<std::pair<double, double>, 4> offsets{};
        if (created.title == kFsdtWingwalkerTitle) {
            offsets = {{{-3.0, 19.0}, {-8.0, 19.0}, {-8.0, 25.0}, {-3.0, 25.0}}};
        } else if (created.title == kFsdtMarshallerTitle) {
            offsets = {{{0.0, 14.0}, {4.0, 14.0}, {4.0, 18.0}, {0.0, 18.0}}};
        } else {
            offsets = {{{4.5, 14.0}, {8.5, 14.0}, {8.5, 18.0}, {4.5, 18.0}}};
        }

        std::array<SIMCONNECT_DATA_WAYPOINT, 4> waypoints{};
        for (std::size_t index = 0; index < waypoints.size(); ++index) {
            const auto position = OffsetPosition(aircraft->second.data, offsets[index].first,
                                                 offsets[index].second);
            waypoints[index].Latitude = position.Latitude;
            waypoints[index].Longitude = position.Longitude;
            waypoints[index].Altitude = position.Altitude;
            waypoints[index].Flags = SIMCONNECT_WAYPOINT_SPEED_REQUESTED | SIMCONNECT_WAYPOINT_ON_GROUND;
            waypoints[index].ktsSpeed = kWorkerWalkingSpeedKnots;
        }
        waypoints.back().Flags |= SIMCONNECT_WAYPOINT_WRAP_TO_FIRST;

        const HRESULT result = SimConnect_SetDataOnSimObject(
            m_simConnect, DefinitionWaypoint, objectId, 0, static_cast<DWORD>(waypoints.size()),
            sizeof(waypoints.front()), waypoints.data());
        if (FAILED(result)) {
            LogLine("SimConnect immediately rejected the walking loop for " + created.title + ".");
            return;
        }

        DWORD packetId = 0;
        if (SUCCEEDED(SimConnect_GetLastSentPacketID(m_simConnect, &packetId))) {
            m_waypointPackets.emplace(packetId, created.title);
        }
        LogLine("Assigned a repeating walking loop to " + created.title + " (ObjectID " +
                std::to_string(objectId) + ").");
    }

    void SpawnFsdtCatering()
    {
        if (!m_simConnect) {
            std::cout << "Not connected to MSFS.\n";
            return;
        }
        if (!m_createdObjects.empty() || !m_pendingCreates.empty()) {
            std::cout << "Service test objects already exist or are being created. Use clearfsdt first.\n";
            return;
        }

        std::size_t aircraftCount = 0;
        std::size_t requested = 0;
        std::size_t rejected = 0;
        for (const auto &[aircraftObjectId, record] : m_aircraft) {
            const AircraftData &aircraft = record.data;
            if (aircraft.isUser != 0 || aircraft.onGround == 0 ||
                std::abs(aircraft.groundSpeedKnots) >= kSafeServiceSpeedKnots) {
                continue;
            }

            ++aircraftCount;
            RequestTestObject(aircraftObjectId, kFsdtCateringTitle,
                              OffsetPosition(aircraft, 0.0, 22.0), requested, rejected);
            RequestTestObject(aircraftObjectId, kBaggageCartTitle,
                              OffsetPosition(aircraft, -8.0, 28.0), requested, rejected);
            RequestTestObject(aircraftObjectId, kFsdtWorkerTitle,
                              OffsetPosition(aircraft, 3.0, 19.0), requested, rejected);
            RequestTestObject(aircraftObjectId, kFsdtWingwalkerTitle,
                              OffsetPosition(aircraft, -3.0, 19.0), requested, rejected);
            RequestTestObject(aircraftObjectId, kFsdtMarshallerTitle,
                              OffsetPosition(aircraft, 0.0, 14.0), requested, rejected);
            RequestTestObject(aircraftObjectId, kAsoboMarshallerTitle,
                              OffsetPosition(aircraft, 4.5, 14.0), requested, rejected);
        }

        std::cout << "Requested " << requested << " test objects for " << aircraftCount
                  << " parked aircraft (FSDT catering, baggage cart, catering worker, "
                      "wingwalker, FSDT marshaller, experimental Asobo marshaller)";
        if (rejected != 0) {
            std::cout << " (" << rejected << " calls rejected immediately)";
        }
        std::cout << ".\n";
    }

    void RequestTestObject(DWORD aircraftObjectId, std::string_view title,
                           const SIMCONNECT_DATA_INITPOSITION &position, std::size_t &requested,
                           std::size_t &rejected)
    {
        const DWORD requestId = m_nextObjectRequestId++;
        const HRESULT result = SimConnect_AICreateSimulatedObject_EX1(
            m_simConnect, title.data(), "", position, requestId);
        if (FAILED(result)) {
            ++rejected;
            return;
        }

        m_pendingCreates.emplace(requestId, PendingCreate{aircraftObjectId, std::string(title)});
        DWORD packetId = 0;
        if (SUCCEEDED(SimConnect_GetLastSentPacketID(m_simConnect, &packetId))) {
            m_createPackets[packetId] = requestId;
        }
        ++requested;
    }

    void ClearCreatedObjects()
    {
        if (!m_simConnect) {
            std::cout << "Not connected to MSFS.\n";
            return;
        }
        if (!m_pendingCreates.empty()) {
            std::cout << "Wait for the pending test-object creations to finish, then retry clearfsdt.\n";
            return;
        }

        std::size_t requested = 0;
        for (const DWORD objectId : m_createdObjects) {
            if (SUCCEEDED(SimConnect_AIRemoveObject(m_simConnect, objectId, m_nextObjectRequestId++))) {
                ++requested;
            }
        }
        m_animatedWorkers.clear();
        m_createdObjects.clear();
        std::cout << "Requested removal of " << requested << " created service test objects.\n";
    }

    void PrintHelp() const
    {
        std::cout << "Commands:\n"
                  << "  status              Connection and scan counts\n"
                  << "  aircraft            List nearby non-user aircraft\n"
                  << "  ground              List nearby ground SimObjects\n"
                  << "  select <ObjectID>   Select an aircraft for inspection\n"
                  << "  target              Show the selected aircraft\n"
                  << "  catering            Request catering for all parked aircraft\n"
                  << "  gpu                  Request power for all parked aircraft\n"
                  << "  baggage              Request baggage for all parked aircraft\n"
                  << "  fsdt                 Spawn a six-object service test per parked aircraft\n"
                  << "  clearfsdt            Remove service test objects created by this probe\n"
                  << "  animprobe <ObjectID> Record animation-driving SimVars to a CSV file\n"
                  << "  stopprobe            Stop and close the animation-probe CSV file\n"
                  << "  scan                 Request fresh snapshots\n"
                  << "  catalog              Write spawnable SimObjects to a text file\n"
                  << "  help                 Show these commands\n"
                  << "  quit                 Exit the probe\n";
    }

    void PrintStatus() const
    {
        const std::size_t nonUserAircraft = static_cast<std::size_t>(std::count_if(
            m_aircraft.begin(), m_aircraft.end(), [](const auto &item) { return item.second.data.isUser == 0; }));
        std::cout << "SimConnect: " << (m_simConnect ? "connected" : "disconnected")
                  << ", non-user aircraft: " << nonUserAircraft << ", ground objects: " << m_ground.size()
                  << ", created test objects: " << m_createdObjects.size()
                  << ", pending creates: " << m_pendingCreates.size() << ", animation probe: ";
        if (m_animationProbeActive) {
            std::cout << "ObjectID " << m_animationProbeObjectId << " (" << m_animationProbeSamples
                      << " samples)";
        } else {
            std::cout << "off";
        }
        std::cout << ", selected: ";
        if (m_selectedObjectId) {
            std::cout << *m_selectedObjectId;
        } else {
            std::cout << "none";
        }
        std::cout << '\n';
    }

    void PrintAircraft() const
    {
        bool found = false;
        const auto now = Clock::now();
        for (const auto &[objectId, record] : m_aircraft) {
            const AircraftData &data = record.data;
            if (data.isUser != 0) {
                continue;
            }
            found = true;
            const auto parkedSeconds = record.parkedSince
                                           ? std::chrono::duration_cast<std::chrono::seconds>(now - *record.parkedSince)
                                                 .count()
                                           : 0;
            const bool stable = record.parkedSince && now - *record.parkedSince >= kStableParkingTime;
            std::cout << "ID=" << objectId;
            if (m_selectedObjectId == objectId) {
                std::cout << " [selected]";
            }
            if (stable && HasRoute(data)) {
                std::cout << " [eligible]";
            } else if (IsInstantlyParked(data)) {
                std::cout << " [parked " << parkedSeconds << "s]";
            }
            std::cout << " phase=" << Phase(data) << " speed=" << std::fixed << std::setprecision(1)
                      << data.groundSpeedKnots << "kt\n"
                      << "  title: " << FixedString(data.title) << '\n'
                      << "  identity: " << FixedString(data.atcAirline) << ' '
                      << FixedString(data.atcFlightNumber) << " / " << FixedString(data.atcId) << '\n'
                      << "  route: " << FixedString(data.fromAirport) << " -> " << FixedString(data.toAirport)
                      << ", current=" << FixedString(data.currentAirport) << '\n'
                      << "  parking: " << FixedString(data.assignedParking) << '\n'
                      << "  state: " << FixedString(data.trafficState) << ", ETD=" << data.etdSeconds
                      << "s, ETA=" << data.etaSeconds << "s\n";
        }
        if (!found) {
            std::cout << "No non-user aircraft are present in the current snapshot.\n";
        }
    }

    void PrintGroundObjects() const
    {
        if (m_ground.empty()) {
            std::cout << "No AI-controlled ground SimObjects are present in the current snapshot.\n";
            return;
        }

        const AircraftData *target = nullptr;
        if (m_selectedObjectId) {
            const auto aircraft = m_aircraft.find(*m_selectedObjectId);
            if (aircraft != m_aircraft.end()) {
                target = &aircraft->second.data;
            }
        }

        for (const auto &[objectId, record] : m_ground) {
            std::cout << "ID=" << objectId << " title=" << FixedString(record.data.title) << " speed=" << std::fixed
                      << std::setprecision(1) << record.data.groundSpeedKnots << "kt";
            if (target) {
                const double distance = DistanceMeters(target->latitude, target->longitude, record.data.latitude,
                                                       record.data.longitude);
                std::cout << " distance-to-target=" << std::setprecision(0) << distance << 'm';
            }
            std::cout << '\n';
        }
    }

    void PrintTarget() const
    {
        if (!m_selectedObjectId) {
            std::cout << "No aircraft selected.\n";
            return;
        }
        const auto aircraft = m_aircraft.find(*m_selectedObjectId);
        if (aircraft == m_aircraft.end()) {
            std::cout << "The selected aircraft is no longer in the current snapshot.\n";
            return;
        }

        const AircraftData &data = aircraft->second.data;
        std::cout << "Target ID=" << *m_selectedObjectId << " title=" << FixedString(data.title)
                  << " onGround=" << (data.onGround ? "yes" : "no") << " speed=" << std::fixed
                  << std::setprecision(1) << data.groundSpeedKnots << "kt route=" << FixedString(data.fromAirport)
                  << "->" << FixedString(data.toAirport) << " state=" << FixedString(data.trafficState) << '\n';
    }

    void LogLine(const std::string &message) const
    {
        std::cout << '\n';
        std::cout << "[probe] " << message << '\n';
        if (!m_quit) {
            PrintPrompt();
            if (!m_commandLine.empty()) {
                std::cout << m_commandLine << std::flush;
            }
        }
    }

    static void PrintPrompt()
    {
        std::cout << "> " << std::flush;
    }

    HANDLE m_simConnect = nullptr;
    bool m_quit = false;
    bool m_reportedWaiting = false;
    bool m_disconnectRequested = false;
    bool m_aircraftRequestPending = false;
    bool m_groundRequestPending = false;
    bool m_animationProbeActive = false;
    Clock::time_point m_aircraftRequestStarted{};
    Clock::time_point m_groundRequestStarted{};
    Clock::time_point m_nextScan{};
    std::optional<DWORD> m_selectedObjectId;
    std::string m_commandLine;
    DWORD m_animationProbeObjectId = 0;
    Clock::time_point m_animationProbeStarted{};
    std::size_t m_animationProbeSamples = 0;
    std::filesystem::path m_animationProbePath;
    std::ofstream m_animationProbeFile;
    std::optional<DWORD> m_animationProbeRequestPacket;
    std::map<DWORD, AircraftRecord> m_aircraft;
    std::map<DWORD, GroundRecord> m_ground;
    std::map<DWORD, AircraftData> m_pendingAircraft;
    std::map<DWORD, GroundData> m_pendingGround;
    std::unordered_map<std::uint64_t, Clock::time_point> m_serviceRequests;
    std::unordered_map<DWORD, SentService> m_sentPackets;
    std::unordered_map<DWORD, DWORD> m_createPackets;
    std::unordered_map<DWORD, std::string> m_waypointPackets;
    std::map<DWORD, AnimatedWorker> m_animatedWorkers;
    std::map<DWORD, PendingCreate> m_pendingCreates;
    std::set<DWORD> m_createdObjects;
    DWORD m_nextObjectRequestId = 10'000;
    std::unordered_map<DWORD, DWORD> m_catalogPackets;
    std::map<DWORD, CatalogBucket> m_catalogs;
    bool m_catalogInProgress = false;
};
} // namespace

int main()
{
    ProbeApp app;
    return app.Run();
}
