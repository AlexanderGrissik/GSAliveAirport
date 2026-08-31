#include "GroundServicesThread.h"

#include "AircraftTrackerThread.h"
#include "AnimationThread.h"
#include "SimConnectIds.h"
#include "SimConnectThread.h"
#include "SimObjectPositioning.h"

#include <algorithm>
#include <cctype>
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
constexpr std::string_view kPassengerBaggageBeltFamily = "PaxBaggageBelt";
constexpr std::string_view kFsdtBaggageBeltPrefix = "FSDT_Tug_660";
constexpr std::string_view kFsdtBaggageWorkerPrefix =
    "FSDT_Baggage_Loader_Man_02";
constexpr std::array<std::string_view, 3> kFsdtBaggageTractorPrefixes{
    "FSDT_TLD_JET_16", "FSDT_Tug_Endurance", "FSDT_Tug_M1A"};
constexpr std::array<double, 3> kFsdtBaggageTractorBackOffsets{
    1.447, 1.47, 1.609};
constexpr std::string_view kFsdtBaggageTowbarTitle =
    "FSDT_BaggageWagon_towbar";
constexpr std::array<std::string_view, 3> kFsdtBaggageWagonTitles{
    "FSDT_BaggageWagon_Open", "FSDT_BaggageWagon_Closed",
    "FSDT_BaggageWagon_Railing"};
constexpr double kFeetToMeters = 0.3048;
constexpr double kBaggageLoaderDoorClearanceMeters = 0.5;

// FSDT_BaggageWagon_Open authors this transform in its Dummy_MAN node. Aligning
// the node with the baggage worker's root puts the wagon at the worker's drop
// side without an aircraft-specific placement offset.
constexpr double kBaggageWagonDummyManRightMeters = -5.442;
constexpr double kBaggageWagonDummyManForwardMeters = -1.029;
constexpr double kBaggageWagonDummyManHeadingDegrees = -90.0;
constexpr double kWorkerWagonTowardWorkerMeters = 0.3;
constexpr double kBaggageWagonFrontTowOffsetMeters = 0.998;
constexpr double kBaggageWagonBackTowOffsetMeters = 1.601;
constexpr double kBaggageWagonGapMeters = 0.45;
constexpr double kBaggageTractorForwardGapMeters = 0.5;
constexpr double kBaggageTrainArcDegrees = 4.0;
constexpr double kFirstBaggageWagonRightOffsetMeters = 0.25;
constexpr double kLastBaggageWagonRightOffsetMeters = -0.25;
constexpr double kPackedLuggagePitchDegrees = 90.0;
// Exact FSDT luggage mesh dimensions. One large and one small case plus the
// requested gap fit the wagon's 1.61 m bed while two large cases do not.
constexpr double kPackedLargeLuggageWidthMeters = 0.8204469084739685;
constexpr double kPackedSmallLuggageWidthMeters = 0.5903337597846985;
constexpr double kPackedSmallLuggageDepthMeters = 0.4132336974143982;
constexpr double kPackedLuggageThicknessMeters = 0.2066168785095215;
constexpr double kPackedLuggageLateralGapMeters = 0.10;
constexpr double kPackedLuggageRowSpacingMeters =
    kPackedSmallLuggageDepthMeters + 0.10;
constexpr double kPackedLuggageOuterRowOffsetMeters =
    kPackedLuggageRowSpacingMeters * 1.5;
constexpr double kPackedLuggageInnerRowOffsetMeters =
    kPackedLuggageRowSpacingMeters * 0.5;
constexpr double kPackedLargeLuggageInnerOffsetMeters =
    (kPackedSmallLuggageWidthMeters + kPackedLuggageLateralGapMeters) / 2.0;
constexpr double kPackedSmallLuggageOuterOffsetMeters =
    (kPackedLargeLuggageWidthMeters + kPackedLuggageLateralGapMeters) / 2.0;

struct LuggageRowPosition
{
    double rightMeters;
    double forwardMeters;
    bool large{};
};

// Each layer has four tightly packed longitudinal rows with two bags across.
// The wider lateral spacing prevents the paired bags from intersecting.
constexpr std::array<LuggageRowPosition, 8> kPackedLuggagePositions{{
    {-kPackedLargeLuggageInnerOffsetMeters,
     -kPackedLuggageOuterRowOffsetMeters, true},
    { kPackedSmallLuggageOuterOffsetMeters,
     -kPackedLuggageOuterRowOffsetMeters, false},
    {-kPackedSmallLuggageOuterOffsetMeters,
     -kPackedLuggageInnerRowOffsetMeters, false},
    { kPackedLargeLuggageInnerOffsetMeters,
     -kPackedLuggageInnerRowOffsetMeters, true},
    {-kPackedLargeLuggageInnerOffsetMeters,
      kPackedLuggageInnerRowOffsetMeters, true},
    { kPackedSmallLuggageOuterOffsetMeters,
      kPackedLuggageInnerRowOffsetMeters, false},
    {-kPackedSmallLuggageOuterOffsetMeters,
      kPackedLuggageOuterRowOffsetMeters, false},
    { kPackedLargeLuggageInnerOffsetMeters,
      kPackedLuggageOuterRowOffsetMeters, true},
}};
constexpr std::array<double, 3> kPackedLuggageLayerHeightsMeters{
    0.73,
    0.73 + kPackedLuggageThicknessMeters + 0.02,
    0.73 + 2.0 * (kPackedLuggageThicknessMeters + 0.02)};
constexpr std::array<std::string_view, 7> kPackedLargeLuggageTitles{
    "FSDT_GSX_Luggage_Large_Black",
    "FSDT_GSX_Luggage_Large_Grey",
    "FSDT_GSX_Luggage_Large_Brown",
    "FSDT_GSX_Luggage_Large_Red",
    "FSDT_GSX_Luggage_Large_Blue",
    "FSDT_GSX_Luggage_Large_LBrown",
    "FSDT_GSX_Luggage_Large_White"};
constexpr std::array<std::string_view, 7> kPackedSmallLuggageTitles{
    "FSDT_GSX_Luggage_Small_Black",
    "FSDT_GSX_Luggage_Small_Grey",
    "FSDT_GSX_Luggage_Small_Brown",
    "FSDT_GSX_Luggage_Small_Red",
    "FSDT_GSX_Luggage_Small_Blue",
    "FSDT_GSX_Luggage_Small_LBrown",
    "FSDT_GSX_Luggage_Small_Green"};
constexpr std::size_t kPackedLuggagePerWagon =
    kPackedLuggagePositions.size() * kPackedLuggageLayerHeightsMeters.size();

bool SafeParkedAircraft(const AircraftSnapshot &aircraft)
{
    return !aircraft.isUser && aircraft.onGround &&
           std::abs(aircraft.groundSpeedKnots) < kRemovalSpeedKnots;
}

bool EqualAsciiIgnoreCase(std::string_view left, std::string_view right)
{
    return left.size() == right.size() &&
        std::ranges::equal(left, right, [](char leftCharacter, char rightCharacter) {
            return std::tolower(static_cast<unsigned char>(leftCharacter)) ==
                   std::tolower(static_cast<unsigned char>(rightCharacter));
        });
}

bool StartsWithAsciiIgnoreCase(std::string_view value, std::string_view prefix)
{
    return value.size() >= prefix.size() &&
        EqualAsciiIgnoreCase(value.substr(0, prefix.size()), prefix);
}

double NormalizeDegrees(double degrees)
{
    return std::fmod(degrees + 360.0, 360.0);
}

std::pair<double, double> RotateRelative(double forwardMeters,
                                         double rightMeters,
                                         double headingDegrees)
{
    const double headingRadians = headingDegrees *
        3.14159265358979323846 / 180.0;
    return {
        std::cos(headingRadians) * forwardMeters -
            std::sin(headingRadians) * rightMeters,
        std::sin(headingRadians) * forwardMeters +
            std::cos(headingRadians) * rightMeters};
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
        if (m_connected) MaintainBaggageBeltAlignments(now);
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

void GroundServicesThread::MaintainBaggageBeltAlignments(
    std::chrono::steady_clock::time_point now)
{
    for (auto &[loaderObjectId, alignment] : m_pendingBaggageBeltAlignments) {
        if (alignment.geometryRequested || now < alignment.geometryRequestDue) continue;
        alignment.geometryRequested = true;
        m_simConnect.RequestBaggageLoaderGeometry(
            loaderObjectId,
            [this, loaderObjectId](BaggageLoaderGeometry geometry) {
                Post([this, loaderObjectId, geometry] {
                    CompleteBaggageBeltAlignment(loaderObjectId, geometry);
                });
            });
    }
}

void GroundServicesThread::CompleteBaggageBeltAlignment(
    AircraftId loaderObjectId, BaggageLoaderGeometry geometry)
{
    const auto pending = m_pendingBaggageBeltAlignments.find(loaderObjectId);
    if (pending == m_pendingBaggageBeltAlignments.end()) return;
    PendingBaggageBeltAlignment &alignment = pending->second;
    alignment.geometryRequested = false;
    if (!geometry.succeeded || !std::isfinite(geometry.angleCurrentDegrees) ||
        !std::isfinite(geometry.endRampYMeters) ||
        !std::isfinite(geometry.endRampZMeters) ||
        !std::isfinite(geometry.pivotYMeters) ||
        !std::isfinite(geometry.pivotZMeters)) {
        m_log("Could not read live ramp geometry for baggage loader ObjectID " +
              std::to_string(loaderObjectId) + "; removing it.");
        const AircraftId aircraftId = alignment.aircraft.objectId;
        m_pendingBaggageBeltAlignments.erase(pending);
        m_simConnect.RemoveObject(loaderObjectId);
        CloseCargoDoor(aircraftId);
        m_createdObjects.erase(loaderObjectId);
        m_baggageLoaderObjects.erase(loaderObjectId);
        m_aircraftByObject.erase(loaderObjectId);
        if (auto group = m_objectsByAircraft.find(aircraftId);
            group != m_objectsByAircraft.end()) {
            group->second.erase(loaderObjectId);
            if (group->second.empty()) m_objectsByAircraft.erase(group);
        }
        PublishStatus();
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (alignment.stage == PendingBaggageBeltAlignment::Stage::MeasureInitialGeometry) {
        const double rampLength = std::hypot(
            geometry.endRampYMeters - geometry.pivotYMeters,
            geometry.endRampZMeters - geometry.pivotZMeters);
        if (rampLength < 0.01) {
            m_log("MSFS returned an invalid ramp length for baggage loader ObjectID " +
                  std::to_string(loaderObjectId) + ".");
            const AircraftId aircraftId = alignment.aircraft.objectId;
            m_pendingBaggageBeltAlignments.erase(pending);
            m_simConnect.RemoveObject(loaderObjectId);
            CloseCargoDoor(aircraftId);
            m_createdObjects.erase(loaderObjectId);
            m_baggageLoaderObjects.erase(loaderObjectId);
            m_aircraftByObject.erase(loaderObjectId);
            if (auto group = m_objectsByAircraft.find(aircraftId);
                group != m_objectsByAircraft.end()) {
                group->second.erase(loaderObjectId);
                if (group->second.empty()) m_objectsByAircraft.erase(group);
            }
            PublishStatus();
            return;
        }
        const double currentPhase = std::atan2(
            geometry.endRampYMeters - geometry.pivotYMeters,
            geometry.endRampZMeters - geometry.pivotZMeters);
        const double desiredPhase = std::asin(std::clamp(
            (alignment.cargoHeightMeters - geometry.pivotYMeters) / rampLength,
            -1.0, 1.0));
        alignment.rampAngleDegrees = std::clamp(
            geometry.angleCurrentDegrees +
                (desiredPhase - currentPhase) * 180.0 / 3.14159265358979323846,
            0.0, 90.0);
        alignment.stage = PendingBaggageBeltAlignment::Stage::WaitForRampTarget;
        alignment.geometryRequestDue = now + 500ms;
        m_simConnect.SetBaggageLoaderRampTarget(loaderObjectId,
                                                 alignment.rampAngleDegrees);
        return;
    }

    if (std::abs(geometry.angleCurrentDegrees - alignment.rampAngleDegrees) > 0.2) {
        alignment.geometryRequestDue = now + 500ms;
        return;
    }

    const double headingRadians = alignment.modelRelativeHeadingDegrees *
        3.14159265358979323846 / 180.0;
    // Keep the ramp endpoint visibly clear of the cargo door while retaining
    // its correct alignment with the selected cargo interactive point.
    const double rampDistanceMeters =
        geometry.endRampZMeters + kBaggageLoaderDoorClearanceMeters;
    const double loaderForwardMeters = alignment.cargoForwardMeters -
        std::cos(headingRadians) * rampDistanceMeters;
    const double loaderRightMeters = alignment.cargoRightMeters -
        std::sin(headingRadians) * rampDistanceMeters;
    auto position = RelativePosition(
        alignment.aircraft.headingDegrees, alignment.aircraft.longitude,
        alignment.aircraft.latitude, alignment.aircraft.altitudeFeet,
        loaderForwardMeters, loaderRightMeters);
    position.Heading = alignment.headingDegrees;
    m_simConnect.SetObjectPosition(loaderObjectId, position);

    PendingCreate worker{};
    worker.aircraft = alignment.aircraft;
    worker.title = std::move(alignment.workerTitle);
    worker.kind = PendingCreateKind::BaggageBeltWorker;
    worker.pairedObjectId = loaderObjectId;
    worker.forwardMeters = loaderForwardMeters;
    worker.rightMeters = loaderRightMeters;
    worker.baggageBeltRampAngleDegrees = alignment.rampAngleDegrees;
    worker.baggageBeltDirection = alignment.direction;
    worker.headingDegrees = alignment.headingDegrees;

    if (!alignment.baggageTrainSelection.wagonTitle.empty()) {
        const double wagonRelativeHeadingDegrees = std::fmod(
            alignment.modelRelativeHeadingDegrees -
                kBaggageWagonDummyManHeadingDegrees + 360.0,
            360.0);
        const double wagonHeadingRadians = wagonRelativeHeadingDegrees *
            3.14159265358979323846 / 180.0;
        const double dummyForwardMeters =
            std::cos(wagonHeadingRadians) *
                kBaggageWagonDummyManForwardMeters -
            std::sin(wagonHeadingRadians) *
                kBaggageWagonDummyManRightMeters;
        const double dummyRightMeters =
            std::sin(wagonHeadingRadians) *
                kBaggageWagonDummyManForwardMeters +
            std::cos(wagonHeadingRadians) *
                kBaggageWagonDummyManRightMeters;
        const double dummyDistanceMeters = std::hypot(
            dummyForwardMeters, dummyRightMeters);
        const double towardWorkerScale = dummyDistanceMeters > 0.001
            ? kWorkerWagonTowardWorkerMeters / dummyDistanceMeters
            : 0.0;
        const double workerWagonForwardMeters = loaderForwardMeters -
            dummyForwardMeters + dummyForwardMeters * towardWorkerScale;
        const double workerWagonRightMeters = loaderRightMeters -
            dummyRightMeters + dummyRightMeters * towardWorkerScale;
        QueueBaggageTrain(loaderObjectId, alignment,
                          workerWagonForwardMeters,
                          workerWagonRightMeters,
                          wagonRelativeHeadingDegrees);
    }

    m_log("Aligned baggage loader ObjectID " + std::to_string(loaderObjectId) +
          " from its live ramp end (Y=" + std::to_string(geometry.endRampYMeters) +
          ", Z=" + std::to_string(geometry.endRampZMeters) +
          ") with " + std::to_string(kBaggageLoaderDoorClearanceMeters) +
          " m door clearance.");
    m_pendingBaggageBeltAlignments.erase(pending);
    QueueCreate(std::move(worker));
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
    std::size_t requestedCount = 0;
    for (const GroundServiceRequest &request : m_serviceRequestBuffer) {
        std::optional<RelativeWalkingPath> walkingPath;
        if (request.walking) {
            walkingPath = RelativeWalkingPath{request.relX1, request.relY1,
                                              request.relX2, request.relY2};
        }
        if (request.family == kPassengerBaggageBeltFamily) {
            if (RequestPassengerBaggageBelt(aircraft, request.title)) {
                ++requestedCount;
            }
        } else {
            RequestObject(aircraft, request.title, request.relY1, request.relX1,
                          walkingPath, request.faceAircraft);
            ++requestedCount;
        }
    }
    m_log("Applied " + std::to_string(requestedCount) +
          " configured " + std::string(AircraftSizeCategoryName(*category)) +
          " service request(s) to aircraft " + std::to_string(aircraft.objectId) +
          " (wingspan " + std::to_string(aircraft.wingSpanMeters) + " m).");
    PublishStatus();
}

void GroundServicesThread::RemoveForAircraft(AircraftId aircraftId, bool closeCargoDoor)
{
    m_configuredAircraft.erase(aircraftId);
    if (closeCargoDoor) CloseCargoDoor(aircraftId);
    for (auto &[token, pending] : m_pendingCreates) {
        if (pending.aircraft.objectId == aircraftId) pending.cancelled = true;
    }
    std::erase_if(m_pendingBaggageBeltAlignments,
                  [aircraftId](const auto &entry) {
                      return entry.second.aircraft.objectId == aircraftId;
                  });
    std::erase_if(m_baggageTrains,
                  [aircraftId](const auto &entry) {
                      return entry.second.aircraft.objectId == aircraftId;
                  });
    const auto group = m_objectsByAircraft.find(aircraftId);
    if (group == m_objectsByAircraft.end()) {
        PublishStatus();
        return;
    }
    const auto objects = group->second;
    m_objectsByAircraft.erase(group);
    for (const AircraftId objectId : objects) {
        m_animation.RemoveObject(objectId);
        m_simConnect.RemoveObject(objectId);
        m_createdObjects.erase(objectId);
        m_baggageLoaderObjects.erase(objectId);
        m_baggageTrainLoaderByObject.erase(objectId);
        m_aircraftByObject.erase(objectId);
    }
    PublishStatus();
    if (!objects.empty()) {
        m_log("Requested removal of " + std::to_string(objects.size()) +
              " services for aircraft " + std::to_string(aircraftId) + ".");
    }
}

void GroundServicesThread::CloseCargoDoor(AircraftId aircraftId)
{
    const auto door = m_openCargoDoorIndices.find(aircraftId);
    if (door == m_openCargoDoorIndices.end()) return;
    m_simConnect.SetCargoDoorOpen(aircraftId, door->second, false);
    m_openCargoDoorIndices.erase(door);
}

void GroundServicesThread::RequestObject(const AircraftSnapshot &aircraft,
                                         std::string title, double forwardMeters,
                                         double rightMeters,
                                         std::optional<RelativeWalkingPath> walkingPath,
                                         bool faceAircraft,
                                         std::optional<double> headingDegrees,
                                         std::optional<std::uint32_t> cargoDoorPointIndex)
{
    PendingCreate pending{};
    pending.aircraft = aircraft;
    pending.title = std::move(title);
    pending.walkingPath = std::move(walkingPath);
    pending.forwardMeters = forwardMeters;
    pending.rightMeters = rightMeters;
    pending.faceAircraft = faceAircraft;
    pending.headingDegrees = headingDegrees;
    pending.cargoDoorPointIndex = cargoDoorPointIndex;
    QueueCreate(std::move(pending));
}

bool GroundServicesThread::RequestPassengerBaggageBelt(
    const AircraftSnapshot &aircraft, std::string title)
{
    if (!aircraft.cargoConnectionPoint) {
        m_log("Skipped PaxBaggageBelt for aircraft " +
              std::to_string(aircraft.objectId) +
              ": MSFS reported no cargo interactive point.");
        return false;
    }
    const AircraftCargoConnectionPoint &connection =
        *aircraft.cargoConnectionPoint;
    const double headingDegrees = std::fmod(
        aircraft.headingDegrees + connection.relativeHeadingDegrees + 540.0,
        360.0);

    if (!title.starts_with(kFsdtBaggageBeltPrefix)) {
        RequestObject(aircraft, std::move(title), connection.forwardMeters,
                      connection.rightMeters, std::nullopt, false,
                      headingDegrees, connection.interactivePointIndex);
        return true;
    }

    const std::string expectedWorkerTitle =
        std::string(kFsdtBaggageWorkerPrefix) +
        title.substr(kFsdtBaggageBeltPrefix.size());
    const auto matchingWorker = std::ranges::find_if(
        m_catalogTitleBuffer, [&expectedWorkerTitle](const std::string &candidate) {
            return EqualAsciiIgnoreCase(candidate, expectedWorkerTitle);
        });
    if (matchingWorker == m_catalogTitleBuffer.end()) {
        m_log("Could not find matching worker " + expectedWorkerTitle + " for " +
              title + "; creating the belt as a static object.");
        RequestObject(aircraft, std::move(title), connection.forwardMeters,
                      connection.rightMeters, std::nullopt, false,
                      headingDegrees, connection.interactivePointIndex);
        return true;
    }

    PendingCreate pending{};
    pending.aircraft = aircraft;
    pending.title = std::move(title);
    pending.kind = PendingCreateKind::BaggageBeltLoader;
    pending.companionTitle = *matchingWorker;
    if (auto trainSelection = SelectBaggageTrain()) {
        pending.baggageTrainSelection = std::move(*trainSelection);
    } else {
        m_log("Could not resolve a complete FSDT baggage train; creating the "
              "animated baggage belt without one.");
    }
    // Initially create at the target, then use the loader's own runtime
    // ramp-end geometry to place its SimObject origin exactly.
    pending.forwardMeters = connection.forwardMeters;
    pending.rightMeters = connection.rightMeters;
    pending.cargoHeightMeters =
        (aircraft.altitudeFeet - aircraft.groundAltitudeFeet) * kFeetToMeters +
        connection.verticalMeters;
    pending.modelRelativeHeadingDegrees = std::fmod(
        connection.relativeHeadingDegrees + 180.0, 360.0);
    std::bernoulli_distribution selectLoading(0.5);
    pending.baggageBeltDirection = selectLoading(m_random)
        ? BaggageBeltDirection::Load
        : BaggageBeltDirection::Unload;
    pending.headingDegrees = headingDegrees;
    pending.cargoDoorPointIndex = connection.interactivePointIndex;
    QueueCreate(std::move(pending));
    return true;
}

std::optional<GroundServicesThread::BaggageTrainSelection>
GroundServicesThread::SelectBaggageTrain()
{
    const auto towbar = std::ranges::find_if(
        m_catalogTitleBuffer, [](const std::string &candidate) {
            return EqualAsciiIgnoreCase(candidate, kFsdtBaggageTowbarTitle);
        });
    if (towbar == m_catalogTitleBuffer.end()) return std::nullopt;

    std::array<std::size_t, kFsdtBaggageTractorPrefixes.size()> tractorCounts{};
    for (const std::string &title : m_catalogTitleBuffer) {
        if (title.find("_CARGO") != std::string::npos) continue;
        for (std::size_t family = 0; family < kFsdtBaggageTractorPrefixes.size();
             ++family) {
            if (StartsWithAsciiIgnoreCase(
                    title, kFsdtBaggageTractorPrefixes[family])) {
                ++tractorCounts[family];
                break;
            }
        }
    }
    std::array<std::size_t, kFsdtBaggageTractorPrefixes.size()>
        availableTractorFamilies{};
    std::size_t availableTractorFamilyCount = 0;
    for (std::size_t family = 0; family < tractorCounts.size(); ++family) {
        if (tractorCounts[family] != 0) {
            availableTractorFamilies[availableTractorFamilyCount++] = family;
        }
    }
    if (availableTractorFamilyCount == 0) return std::nullopt;

    std::uniform_int_distribution<std::size_t> selectTractorFamily(
        0, availableTractorFamilyCount - 1);
    const std::size_t tractorFamily =
        availableTractorFamilies[selectTractorFamily(m_random)];
    std::uniform_int_distribution<std::size_t> selectTractorTitle(
        0, tractorCounts[tractorFamily] - 1);
    std::size_t selectedTractorIndex = selectTractorTitle(m_random);
    const std::string *tractorTitle = nullptr;
    for (const std::string &candidate : m_catalogTitleBuffer) {
        if (candidate.find("_CARGO") != std::string::npos ||
            !StartsWithAsciiIgnoreCase(
                candidate, kFsdtBaggageTractorPrefixes[tractorFamily])) {
            continue;
        }
        if (selectedTractorIndex-- == 0) {
            tractorTitle = &candidate;
            break;
        }
    }
    if (!tractorTitle) return std::nullopt;

    std::array<const std::string *, kFsdtBaggageWagonTitles.size()>
        availableWagons{};
    std::size_t availableWagonCount = 0;
    for (const std::string_view wanted : kFsdtBaggageWagonTitles) {
        const auto wagon = std::ranges::find_if(
            m_catalogTitleBuffer, [wanted](const std::string &candidate) {
                return EqualAsciiIgnoreCase(candidate, wanted);
            });
        if (wagon != m_catalogTitleBuffer.end()) {
            availableWagons[availableWagonCount++] = &*wagon;
        }
    }
    if (availableWagonCount == 0) return std::nullopt;
    std::uniform_int_distribution<std::size_t> selectWagon(
        0, availableWagonCount - 1);

    BaggageTrainSelection selection{};
    selection.tractorTitle = *tractorTitle;
    selection.tractorBackOffsetMeters =
        kFsdtBaggageTractorBackOffsets[tractorFamily];
    selection.towbarTitle = *towbar;
    selection.wagonTitle = *availableWagons[selectWagon(m_random)];
    return selection;
}

void GroundServicesThread::QueueBaggageTrain(
    AircraftId loaderObjectId,
    const PendingBaggageBeltAlignment &alignment,
    double workerWagonForwardMeters,
    double workerWagonRightMeters,
    double workerWagonRelativeHeadingDegrees)
{
    const BaggageTrainSelection &selection = alignment.baggageTrainSelection;
    if (selection.tractorTitle.empty() || selection.towbarTitle.empty() ||
        selection.wagonTitle.empty()) {
        return;
    }

    struct TrainPose
    {
        double forwardMeters{};
        double rightMeters{};
        double relativeHeadingDegrees{};
    };

    const auto direction = [](double headingDegrees) {
        const double radians = headingDegrees *
            3.14159265358979323846 / 180.0;
        return std::pair{std::cos(radians), std::sin(radians)};
    };
    const auto bisectDirections = [](const auto &first, const auto &second) {
        const double forward = first.first + second.first;
        const double right = first.second + second.second;
        const double length = std::hypot(forward, right);
        return std::pair{forward / length, right / length};
    };
    const auto directionHeading = [](const auto &value) {
        constexpr double radiansToDegrees =
            180.0 / 3.14159265358979323846;
        return NormalizeDegrees(
            std::atan2(value.second, value.first) * radiansToDegrees);
    };
    // The wagon aligned to the worker is first behind the tractor. Each later
    // wagon is chained from the preceding rear hitch, with a small turn that
    // keeps the train natural without opening visible gaps between hitches.
    const TrainPose workerWagon{workerWagonForwardMeters,
                                workerWagonRightMeters,
                                NormalizeDegrees(
                                    workerWagonRelativeHeadingDegrees)};
    const auto workerDirection = direction(workerWagon.relativeHeadingDegrees);
    const double secondHeading = NormalizeDegrees(
        workerWagon.relativeHeadingDegrees + kBaggageTrainArcDegrees);
    const auto secondDirection = direction(secondHeading);
    const auto workerSecondBisector = bisectDirections(
        workerDirection, secondDirection);
    const double workerRearForward = workerWagon.forwardMeters -
        workerDirection.first * kBaggageWagonBackTowOffsetMeters;
    const double workerRearRight = workerWagon.rightMeters -
        workerDirection.second * kBaggageWagonBackTowOffsetMeters;
    const double secondFrontForward = workerRearForward -
        workerSecondBisector.first * kBaggageWagonGapMeters;
    const double secondFrontRight = workerRearRight -
        workerSecondBisector.second * kBaggageWagonGapMeters;
    const TrainPose secondWagon{
        secondFrontForward -
            secondDirection.first * kBaggageWagonFrontTowOffsetMeters,
        secondFrontRight -
            secondDirection.second * kBaggageWagonFrontTowOffsetMeters,
        secondHeading};

    const double thirdHeading = NormalizeDegrees(
        secondHeading + kBaggageTrainArcDegrees);
    const auto thirdDirection = direction(thirdHeading);
    const auto secondThirdBisector = bisectDirections(
        secondDirection, thirdDirection);
    const double secondRearForward = secondWagon.forwardMeters -
        secondDirection.first * kBaggageWagonBackTowOffsetMeters;
    const double secondRearRight = secondWagon.rightMeters -
        secondDirection.second * kBaggageWagonBackTowOffsetMeters;
    const double thirdFrontForward = secondRearForward -
        secondThirdBisector.first * kBaggageWagonGapMeters;
    const double thirdFrontRight = secondRearRight -
        secondThirdBisector.second * kBaggageWagonGapMeters;
    const TrainPose thirdWagon{
        thirdFrontForward -
            thirdDirection.first * kBaggageWagonFrontTowOffsetMeters,
        thirdFrontRight -
            thirdDirection.second * kBaggageWagonFrontTowOffsetMeters,
        thirdHeading};

    const double tractorHeading = NormalizeDegrees(
        workerWagon.relativeHeadingDegrees - kBaggageTrainArcDegrees);
    const auto tractorDirection = direction(tractorHeading);
    const double workerFrontForward = workerWagon.forwardMeters +
        workerDirection.first * kBaggageWagonFrontTowOffsetMeters;
    const double workerFrontRight = workerWagon.rightMeters +
        workerDirection.second * kBaggageWagonFrontTowOffsetMeters;
    const TrainPose tractor{
        workerFrontForward + tractorDirection.first *
            (selection.tractorBackOffsetMeters +
             kBaggageTractorForwardGapMeters),
        workerFrontRight + tractorDirection.second *
            (selection.tractorBackOffsetMeters +
             kBaggageTractorForwardGapMeters),
        tractorHeading};

    std::array<TrainPose, 3> wagonPoses{
        workerWagon, secondWagon, thirdWagon};
    wagonPoses.front().rightMeters +=
        kFirstBaggageWagonRightOffsetMeters;
    wagonPoses.back().rightMeters +=
        kLastBaggageWagonRightOffsetMeters;
    const auto tractorWorkerBisector = bisectDirections(
        tractorDirection, workerDirection);
    const std::array<double, 3> towbarHeadings{
        directionHeading(tractorWorkerBisector),
        directionHeading(workerSecondBisector),
        directionHeading(secondThirdBisector)};

    BaggageTrain train{};
    train.aircraft = alignment.aircraft;
    for (std::size_t index = 0; index < wagonPoses.size(); ++index) {
        train.wagons[index].forwardMeters = wagonPoses[index].forwardMeters;
        train.wagons[index].rightMeters = wagonPoses[index].rightMeters;
        train.wagons[index].relativeHeadingDegrees =
            wagonPoses[index].relativeHeadingDegrees;
    }
    m_baggageTrains[loaderObjectId] = std::move(train);

    const auto queueComponent = [this, loaderObjectId, &alignment](
                                    std::string title, const TrainPose &pose,
                                    std::size_t baggageWagonIndex = 3) {
        PendingCreate component{};
        component.aircraft = alignment.aircraft;
        component.title = std::move(title);
        component.kind = PendingCreateKind::BaggageTrainComponent;
        component.pairedObjectId = loaderObjectId;
        component.forwardMeters = pose.forwardMeters;
        component.rightMeters = pose.rightMeters;
        component.headingDegrees = NormalizeDegrees(
            alignment.aircraft.headingDegrees + pose.relativeHeadingDegrees);
        component.baggageWagonIndex = baggageWagonIndex;
        QueueCreate(std::move(component));
    };

    queueComponent(selection.tractorTitle, tractor);
    for (std::size_t index = 0; index < wagonPoses.size(); ++index) {
        const TrainPose &pose = wagonPoses[index];
        // The towbar is the wagon's separate front steering/drawbar assembly,
        // so it shares the body origin. Aim it down the real hitch-to-hitch
        // bisector; its authored forward axis is opposite that direction.
        TrainPose towbarPose = pose;
        towbarPose.relativeHeadingDegrees = NormalizeDegrees(
            towbarHeadings[index] + 180.0);
        queueComponent(selection.towbarTitle, towbarPose);
        queueComponent(selection.wagonTitle, pose, index);
    }

    m_log("Requested curved baggage train using " + selection.tractorTitle +
          " and three permanently full " + selection.wagonTitle +
          " wagons, with the worker wagon first, for loader ObjectID " +
          std::to_string(loaderObjectId) + ".");
}

void GroundServicesThread::RequestBaggageLuggage(
    AircraftId loaderObjectId, std::size_t luggageIndex)
{
    const auto trainEntry = m_baggageTrains.find(loaderObjectId);
    if (trainEntry == m_baggageTrains.end() ||
        luggageIndex >= trainEntry->second.luggageObjectIds.size()) {
        return;
    }
    BaggageTrain &train = trainEntry->second;
    const std::size_t wagonIndex =
        luggageIndex / kPackedLuggagePerWagon;
    const std::size_t slotIndex =
        luggageIndex % kPackedLuggagePerWagon;
    const std::size_t layerIndex =
        slotIndex / kPackedLuggagePositions.size();
    const std::size_t positionIndex =
        slotIndex % kPackedLuggagePositions.size();
    const BaggageWagon &wagon = train.wagons[wagonIndex];
    if (wagon.objectId == 0 ||
        train.luggageObjectIds[luggageIndex] != 0 ||
        train.luggagePending[luggageIndex]) {
        return;
    }

    const LuggageRowPosition &position =
        kPackedLuggagePositions[positionIndex];
    const auto [slotForward, slotRight] = RotateRelative(
        position.forwardMeters, position.rightMeters,
        wagon.relativeHeadingDegrees);
    PendingCreate luggage{};
    luggage.aircraft = train.aircraft;
    if (position.large) {
        luggage.title = std::string(kPackedLargeLuggageTitles[
            slotIndex % kPackedLargeLuggageTitles.size()]);
    } else {
        luggage.title = std::string(kPackedSmallLuggageTitles[
            slotIndex % kPackedSmallLuggageTitles.size()]);
    }
    luggage.kind = PendingCreateKind::BaggageLuggage;
    luggage.pairedObjectId = loaderObjectId;
    luggage.luggageIndex = luggageIndex;
    luggage.forwardMeters = wagon.forwardMeters + slotForward;
    luggage.rightMeters = wagon.rightMeters + slotRight;
    luggage.altitudeFeet = train.aircraft.groundAltitudeFeet +
        kPackedLuggageLayerHeightsMeters[layerIndex] / kFeetToMeters;
    luggage.onGround = false;
    luggage.pitchDegrees = kPackedLuggagePitchDegrees;
    luggage.headingDegrees = NormalizeDegrees(
        train.aircraft.headingDegrees +
            wagon.relativeHeadingDegrees + 180.0);
    train.luggagePending[luggageIndex] = true;
    QueueCreate(std::move(luggage));
}

void GroundServicesThread::RemoveBaggageLuggage(
    AircraftId loaderObjectId, std::size_t luggageIndex)
{
    const auto trainEntry = m_baggageTrains.find(loaderObjectId);
    if (trainEntry == m_baggageTrains.end() ||
        luggageIndex >= trainEntry->second.luggageObjectIds.size()) {
        return;
    }
    BaggageTrain &train = trainEntry->second;
    if (train.luggagePending[luggageIndex]) {
        for (auto &[token, pending] : m_pendingCreates) {
            if (pending.kind == PendingCreateKind::BaggageLuggage &&
                pending.pairedObjectId == loaderObjectId &&
                pending.luggageIndex == luggageIndex) {
                pending.cancelled = true;
            }
        }
        train.luggagePending[luggageIndex] = false;
    }
    const AircraftId objectId = train.luggageObjectIds[luggageIndex];
    if (objectId == 0) return;
    train.luggageObjectIds[luggageIndex] = 0;
    m_baggageTrainLoaderByObject.erase(objectId);
    m_simConnect.RemoveObject(objectId);
    m_createdObjects.erase(objectId);
    m_aircraftByObject.erase(objectId);
    if (auto group = m_objectsByAircraft.find(train.aircraft.objectId);
        group != m_objectsByAircraft.end()) {
        group->second.erase(objectId);
        if (group->second.empty()) m_objectsByAircraft.erase(group);
    }
    PublishStatus();
}

void GroundServicesThread::QueueCreate(PendingCreate pending)
{
    const std::uint64_t token = m_nextCreateToken++;
    const std::string title = pending.title;
    auto position = RelativePosition(
        pending.aircraft.headingDegrees, pending.aircraft.longitude,
        pending.aircraft.latitude,
        pending.onGround ? pending.aircraft.altitudeFeet : pending.altitudeFeet,
        pending.forwardMeters, pending.rightMeters);
    position.OnGround = pending.onGround ? 1 : 0;
    position.Pitch = pending.pitchDegrees;
    position.Bank = pending.bankDegrees;
    if (pending.headingDegrees) {
        position.Heading = *pending.headingDegrees;
    } else if (pending.faceAircraft) {
        position.Heading = HeadingTowardRelativeOrigin(
            pending.aircraft.headingDegrees, pending.forwardMeters,
            pending.rightMeters);
    }
    m_pendingCreates.emplace(token, std::move(pending));
    PublishStatus();
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
    const auto clearLuggagePending = [this, &created] {
        if (created.kind != PendingCreateKind::BaggageLuggage) {
            return;
        }
        if (auto train = m_baggageTrains.find(created.pairedObjectId);
            train != m_baggageTrains.end() &&
            created.luggageIndex < train->second.luggagePending.size()) {
            train->second.luggagePending[created.luggageIndex] = false;
        }
    };
    if (objectId == 0) {
        clearLuggagePending();
        PublishStatus();
        return;
    }
    if (created.cancelled) {
        clearLuggagePending();
        m_simConnect.RemoveObject(objectId);
        PublishStatus();
        m_log("Immediately removed late-created " + created.title +
              " for inactive aircraft " + std::to_string(created.aircraft.objectId) + ".");
        return;
    }
    if (created.kind == PendingCreateKind::BaggageBeltWorker ||
        created.kind == PendingCreateKind::BaggageTrainComponent ||
        created.kind == PendingCreateKind::BaggageLuggage) {
        const auto loaderOwner = m_aircraftByObject.find(created.pairedObjectId);
        if (!m_createdObjects.contains(created.pairedObjectId) ||
            loaderOwner == m_aircraftByObject.end() ||
            loaderOwner->second != created.aircraft.objectId ||
            (created.kind != PendingCreateKind::BaggageBeltWorker &&
             !m_baggageTrains.contains(created.pairedObjectId))) {
            clearLuggagePending();
            m_simConnect.RemoveObject(objectId);
            PublishStatus();
            m_log("Immediately removed late-created baggage companion " +
                  created.title + " because its belt ObjectID " +
                  std::to_string(created.pairedObjectId) + " is no longer active.");
            return;
        }
    }
    m_createdObjects.insert(objectId);
    m_objectsByAircraft[created.aircraft.objectId].insert(objectId);
    m_aircraftByObject[objectId] = created.aircraft.objectId;
    if (created.kind == PendingCreateKind::BaggageTrainComponent ||
        created.kind == PendingCreateKind::BaggageLuggage) {
        m_baggageTrainLoaderByObject[objectId] = created.pairedObjectId;
    }
    if (created.cargoDoorPointIndex) {
        m_baggageLoaderObjects.insert(objectId);
        m_openCargoDoorIndices[created.aircraft.objectId] =
            *created.cargoDoorPointIndex;
        m_simConnect.SetCargoDoorOpen(created.aircraft.objectId,
                                      *created.cargoDoorPointIndex, true);
        m_log("Instantly requested rear cargo-door opening for aircraft " +
              std::to_string(created.aircraft.objectId) +
              " after creating loader ObjectID " + std::to_string(objectId) +
              " at interactive point " +
              std::to_string(*created.cargoDoorPointIndex) + ".");
    }
    if (created.kind == PendingCreateKind::BaggageBeltLoader) {
        m_simConnect.FreezeObject(objectId);
        PendingBaggageBeltAlignment alignment{};
        alignment.aircraft = created.aircraft;
        alignment.workerTitle = created.companionTitle;
        alignment.baggageTrainSelection = created.baggageTrainSelection;
        alignment.cargoForwardMeters = created.forwardMeters;
        alignment.cargoRightMeters = created.rightMeters;
        alignment.cargoHeightMeters = created.cargoHeightMeters;
        alignment.modelRelativeHeadingDegrees = created.modelRelativeHeadingDegrees;
        alignment.headingDegrees = *created.headingDegrees;
        alignment.direction = created.baggageBeltDirection;
        alignment.geometryRequestDue = std::chrono::steady_clock::now() + 250ms;
        m_pendingBaggageBeltAlignments.emplace(objectId, std::move(alignment));
        m_simConnect.SetBaggageLoaderRampTarget(objectId, 0.0);
    } else if (created.kind == PendingCreateKind::BaggageBeltWorker) {
        m_animation.AddBaggageBelt(created.pairedObjectId, objectId,
                                   created.baggageBeltRampAngleDegrees,
                                   created.baggageBeltDirection);
    } else if (created.kind == PendingCreateKind::BaggageTrainComponent) {
        m_simConnect.FreezeObject(objectId);
        if (created.baggageWagonIndex < 3) {
            if (auto train = m_baggageTrains.find(created.pairedObjectId);
                train != m_baggageTrains.end()) {
                train->second.wagons[created.baggageWagonIndex].objectId =
                    objectId;
                const std::size_t firstLuggageIndex =
                    created.baggageWagonIndex * kPackedLuggagePerWagon;
                for (std::size_t slotIndex = 0;
                     slotIndex < kPackedLuggagePerWagon; ++slotIndex) {
                    RequestBaggageLuggage(
                        created.pairedObjectId,
                        firstLuggageIndex + slotIndex);
                }
            }
        }
    } else if (created.kind == PendingCreateKind::BaggageLuggage) {
        clearLuggagePending();
        if (auto train = m_baggageTrains.find(created.pairedObjectId);
            train != m_baggageTrains.end() &&
            created.luggageIndex < train->second.luggageObjectIds.size()) {
            train->second.luggageObjectIds[created.luggageIndex] = objectId;
        }
        m_simConnect.FreezeObject(objectId);
    } else if (created.walkingPath) {
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
        // The target aircraft has already gone, so only clear our door state;
        // sending a close event to its stale ObjectID would be meaningless.
        m_openCargoDoorIndices.erase(objectId);
        RemoveForAircraft(objectId, false);
        return;
    }
    for (auto &[token, pending] : m_pendingCreates) {
        if (pending.pairedObjectId == objectId) pending.cancelled = true;
    }
    m_pendingBaggageBeltAlignments.erase(objectId);
    if (const auto trainOwner = m_baggageTrainLoaderByObject.find(objectId);
        trainOwner != m_baggageTrainLoaderByObject.end()) {
        if (auto train = m_baggageTrains.find(trainOwner->second);
            train != m_baggageTrains.end()) {
            const auto wagon = std::ranges::find_if(
                train->second.wagons,
                [objectId](const BaggageWagon &candidate) {
                    return candidate.objectId == objectId;
                });
            if (wagon != train->second.wagons.end()) {
                const std::size_t wagonIndex = static_cast<std::size_t>(
                    std::distance(train->second.wagons.begin(), wagon));
                wagon->objectId = 0;
                const std::size_t firstLuggageIndex =
                    wagonIndex * kPackedLuggagePerWagon;
                for (std::size_t slotIndex = 0;
                     slotIndex < kPackedLuggagePerWagon; ++slotIndex) {
                    RemoveBaggageLuggage(
                        trainOwner->second,
                        firstLuggageIndex + slotIndex);
                }
            } else {
                for (std::size_t index = 0;
                     index < train->second.luggageObjectIds.size(); ++index) {
                    if (train->second.luggageObjectIds[index] == objectId) {
                        train->second.luggageObjectIds[index] = 0;
                        train->second.luggagePending[index] = false;
                        break;
                    }
                }
            }
        }
        m_baggageTrainLoaderByObject.erase(trainOwner);
    }
    const bool wasCreated = m_createdObjects.contains(objectId);
    const bool wasBaggageLoader = m_baggageLoaderObjects.erase(objectId) != 0;
    m_animation.RemoveObject(objectId);
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
    if (wasBaggageLoader && ownerId != 0) {
        RemoveForAircraft(ownerId);
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
    m_pendingBaggageBeltAlignments.clear();
    m_baggageTrains.clear();
    m_baggageTrainLoaderByObject.clear();
    m_createdObjects.clear();
    m_baggageLoaderObjects.clear();
    m_configuredAircraft.clear();
    m_objectsByAircraft.clear();
    m_aircraftByObject.clear();
    m_openCargoDoorIndices.clear();
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
    m_pendingBaggageBeltAlignments.clear();
    for (const auto &[aircraftId, pointIndex] : m_openCargoDoorIndices) {
        m_simConnect.SetCargoDoorOpen(aircraftId, pointIndex, false);
    }
    m_openCargoDoorIndices.clear();
    const auto objectCount = m_createdObjects.size();
    for (const AircraftId objectId : m_createdObjects) {
        m_animation.RemoveObject(objectId);
        m_simConnect.RemoveObject(objectId);
    }
    m_createdObjects.clear();
    m_baggageLoaderObjects.clear();
    m_baggageTrains.clear();
    m_baggageTrainLoaderByObject.clear();
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
