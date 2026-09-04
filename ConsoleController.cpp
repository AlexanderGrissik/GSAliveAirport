#include "ConsoleController.h"

#include <conio.h>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

namespace parking_services
{
namespace
{
constexpr SIMCONNECT_DATA_DEFINITION_ID kGroundDefinition = 2;

std::string Lower(std::string value)
{
    std::ranges::transform(value, value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool HasRoute(const AircraftSnapshot &aircraft)
{
    return !aircraft.fromAirport.empty() && !aircraft.toAirport.empty();
}

void PrintCargoPoint(std::ostream &output, std::string_view name,
                     const std::optional<AircraftCargoConnectionPoint> &point)
{
    output << ' ' << name << '=';
    if (!point) {
        output << "none";
        return;
    }
    output << "(forward=" << std::setprecision(1) << point->forwardMeters
           << "m,right=" << point->rightMeters << "m,vertical="
           << point->verticalMeters << "m)";
}

std::string Phase(const AircraftSnapshot &aircraft)
{
    const std::string state = NormalizeTrafficState(aircraft.trafficState);
    const std::string current = Lower(aircraft.currentAirport);
    const std::string origin = Lower(aircraft.fromAirport);
    const std::string destination = Lower(aircraft.toAirport);
    if (state == "shutdown" || state == "postflight support" ||
        (!destination.empty() && current == destination && current != origin)) return "arrival";
    if (state == "flt plan" || state == "startup" || state == "preflight support" ||
        state == "clearance" || (!origin.empty() && current == origin && current != destination)) {
        return "departure";
    }
    if (state.find("push back") != std::string::npos ||
        state.find("taxi") != std::string::npos) return "moving";
    if ((state == "sleep" || state.empty()) && !HasRoute(aircraft)) return "static/unknown";
    return "ambiguous";
}
}

void ConsoleController::Pump(const std::function<void(AppCommand)> &handler)
{
    while (_kbhit()) {
        const int key = _getch();
        if (key == 0 || key == 224) {
            const int extendedKey = _getch();
            if (extendedKey == 72 && !m_lastCommand.empty()) {
                for (std::size_t index = 0; index < m_cursor; ++index) {
                    std::cout << '\b';
                }
                std::cout << std::string(m_line.size(), ' ');
                for (std::size_t index = 0; index < m_line.size(); ++index) {
                    std::cout << '\b';
                }
                m_line = m_lastCommand;
                m_cursor = m_line.size();
                std::cout << m_line << std::flush;
            } else if (extendedKey == 75 && m_cursor != 0) {
                --m_cursor;
                std::cout << '\b' << std::flush;
            } else if (extendedKey == 77 && m_cursor < m_line.size()) {
                std::cout << m_line[m_cursor++] << std::flush;
            }
            continue;
        }
        if (key == '\r') {
            std::cout << m_line.substr(m_cursor) << '\n';
            const std::string line = std::exchange(m_line, {});
            m_cursor = 0;
            if (!line.empty()) m_lastCommand = line;
            handler(Parse(line));
        } else if (key == '\b') {
            if (m_cursor != 0) {
                m_line.erase(--m_cursor, 1);
                const std::string_view suffix(m_line.data() + m_cursor,
                                              m_line.size() - m_cursor);
                std::cout << '\b' << suffix << ' ';
                for (std::size_t index = 0; index <= suffix.size(); ++index) {
                    std::cout << '\b';
                }
                std::cout << std::flush;
            }
        } else if (key >= 32 && key <= 126) {
            const char character = static_cast<char>(key);
            m_line.insert(m_cursor, 1, character);
            const std::string_view suffix(m_line.data() + m_cursor,
                                          m_line.size() - m_cursor);
            ++m_cursor;
            std::cout << suffix;
            for (std::size_t index = 1; index < suffix.size(); ++index) {
                std::cout << '\b';
            }
            std::cout << std::flush;
        }
    }
}

AppCommand ConsoleController::Parse(std::string_view line)
{
    std::istringstream input{std::string(line)};
    std::string command;
    input >> command;
    command = Lower(command);
    if (command.empty()) return {};
    if (command == "quit" || command == "exit") return {AppCommandType::Quit};
    if (command == "help" || command == "?") return {AppCommandType::Help};
    if (command == "status") return {AppCommandType::Status};
    if (command == "tracked") return {AppCommandType::Tracked};
    if (command == "aircraft1" || command == "radius1") return {AppCommandType::Aircraft1};
    if (command == "aircraft5" || command == "radius5") return {AppCommandType::Aircraft5};
    if (command == "parked") return {AppCommandType::Parked};
    if (command == "ground") return {AppCommandType::Ground};
    if (command == "roads") {
        std::string icao;
        if (!(input >> icao) || icao.empty()) return {AppCommandType::Unknown};
        std::ranges::transform(icao, icao.begin(),
                               [](unsigned char c) {
                                   return static_cast<char>(std::toupper(c));
                               });
        AppCommand result;
        result.type = AppCommandType::Roads;
        result.argument = icao;
        return result;
    }
    if (command == "log") return {AppCommandType::Log};
    if (command == "reset") return {AppCommandType::Reset};
    if (command == "reload") return {AppCommandType::Reload};
    return {AppCommandType::Unknown};
}

void ConsoleController::PrintHelp()
{
    std::cout << "Commands:\n"
              << "  status              Connection and component counts\n"
              << "  tracked             Retained aircraft (admitted inside 1 km)\n"
              << "  aircraft1           Aircraft currently within 1 km\n"
              << "  aircraft5           Aircraft currently within 5 km\n"
              << "  parked              Detailed list excluding STATE_SIMPLE_TAXI\n"
              << "  ground              Request one 5 km ground-object debug list\n"
              << "  roads <ICAO>        List non-aircraft roads (TYPE 6/7) and endpoints\n"
              << "  log                 Toggle background event/periodic logging\n"
              << "  reset               Clear tracking and created objects\n"
              << "  reload              Reread the JSON config and rebuild services\n"
              << "  help | quit\n";
}

void ConsoleController::PrintPrompt()
{
    std::cout << "> " << std::flush;
}

void ConsoleController::PrintSnapshots(const std::vector<AircraftSnapshot> &aircraft,
                                       std::string_view label)
{
    std::cout << aircraft.size() << ' ' << label << ":\n";
    for (const auto &data : aircraft) {
        std::cout << "ID=" << data.objectId << " distance=" << std::fixed
                  << std::setprecision(0) << data.distanceFromUserMeters << "m speed="
                  << std::setprecision(1) << data.groundSpeedKnots << "kt wingspan="
                  << data.wingSpanMeters << "m title="
                  << data.title << " state=" << data.trafficState << " nav="
                  << data.lightNav << " on-ground=" << data.onGround;
        PrintCargoPoint(std::cout, "cargo-front", data.cargoDoorRightFront);
        PrintCargoPoint(std::cout, "cargo-back", data.cargoDoorRightBack);
        std::cout << '\n';
    }
}

void ConsoleController::PrintParked(const std::vector<AircraftSnapshot> &aircraft)
{
    bool found = false;
    const auto now = TrackerClock::now();
    for (const auto &data : aircraft) {
        if (NormalizeTrafficState(data.trafficState) == "simple taxi") continue;
        found = true;
        const auto parkedSeconds = data.parkedSince
            ? std::chrono::duration_cast<std::chrono::seconds>(now - *data.parkedSince).count() : 0;
        const bool eligible = data.onGround && data.lightNav &&
                              std::abs(data.groundSpeedKnots) < 1.0 &&
                              NormalizeTrafficState(data.trafficState) != "simple taxi";
        std::cout << "ID=" << data.objectId;
        if (eligible) std::cout << " [service-candidate]";
        else if (data.parkedSince) std::cout << " [parked " << parkedSeconds << "s]";
        std::cout << " phase=" << Phase(data) << " distance=" << std::fixed
                  << std::setprecision(0) << data.distanceFromUserMeters << "m speed="
                  << std::setprecision(1) << data.groundSpeedKnots << "kt wingspan="
                  << data.wingSpanMeters << "m\n"
                  << "  title: " << data.title << '\n'
                  << "  identity: " << data.atcAirline << ' ' << data.atcFlightNumber
                  << " / " << data.atcId << '\n'
                  << "  route: " << data.fromAirport << " -> " << data.toAirport
                  << ", current=" << data.currentAirport << '\n'
                  << "  parking: " << data.assignedParking << ", runway=" << data.assignedRunway
                  << ", IFR=" << (data.isIfr ? "yes" : "no") << '\n'
                  << "  state: " << data.trafficState << ", ETD=" << data.etdSeconds
                  << "s, ETA=" << data.etaSeconds << "s\n"
                  << "  signals: beacon=" << data.lightBeacon << ", nav=" << data.lightNav
                  << ", taxi=" << data.lightTaxi << ", strobe=" << data.lightStrobe
                  << ", parking-brake=" << data.parkingBrake
                  << ", pushback-attached=" << data.pushbackAttached
                  << ", pushback-wait=" << data.pushbackWait
                  << ", transponder=" << data.transponderState << '\n';
        const auto printDetailedCargo = [](std::string_view name,
                                           const std::optional<AircraftCargoConnectionPoint> &point) {
            std::cout << "  " << name << ": ";
            if (!point) {
                std::cout << "none\n";
                return;
            }
            std::cout << "forward=" << std::fixed << std::setprecision(1)
                      << point->forwardMeters << "m, right=" << point->rightMeters
                      << "m, vertical=" << point->verticalMeters << "m, heading="
                      << point->relativeHeadingDegrees << "deg relative\n";
        };
        printDetailedCargo("cargo-door-right-front", data.cargoDoorRightFront);
        printDetailedCargo("cargo-door-right-back", data.cargoDoorRightBack);
    }
    if (!found) {
        std::cout << "No aircraft outside STATE_SIMPLE_TAXI are present.\n";
    }
}

void ConsoleController::PrintGround(const std::vector<GroundSnapshot> &ground)
{
    if (ground.empty()) {
        std::cout << "No AI-controlled ground SimObjects were returned.\n";
        return;
    }
    for (const auto &object : ground) {
        std::cout << "ID=" << object.objectId << " title=" << object.title << " speed="
                  << std::fixed << std::setprecision(1) << object.groundSpeedKnots << "kt";
        std::cout << '\n';
    }
}

void ConsoleController::PrintStatus(const ConsoleStatus &status) const
{
    std::cout << "tracked aircraft: " << status.trackedAircraft
              << ", nearby aircraft: " << status.nearbyAircraft
              << ", last ground debug objects: " << status.groundObjects
              << ", created services: " << status.createdServices
              << ", pending creates: " << status.pendingCreates
              << ", moving animated objects: " << status.movingObjects << '/'
              << status.maximumMovingObjects;
    std::cout << '\n';
}

ConsoleController::ConsoleController(ISimConnectHandler &simConnect)
    : m_simConnect(simConnect)
{
}

void ConsoleController::OnSimConnected()
{
    m_simConnect.AddDatum(kGroundDefinition, "TITLE", "",
                          SIMCONNECT_DATATYPE_STRING256, NewSetupRequest());
    m_simConnect.AddDatum(kGroundDefinition, "PLANE LATITUDE", "degrees",
                          SIMCONNECT_DATATYPE_FLOAT64, NewSetupRequest());
    m_simConnect.AddDatum(kGroundDefinition, "PLANE LONGITUDE", "degrees",
                          SIMCONNECT_DATATYPE_FLOAT64, NewSetupRequest());
    m_simConnect.AddDatum(kGroundDefinition, "GROUND VELOCITY", "knots",
                          SIMCONNECT_DATATYPE_FLOAT64, NewSetupRequest());
}

void ConsoleController::RequestGroundDebugSnapshot()
{
    if (m_groundRequest) {
        std::cout << "A ground-object debug request is already in progress.\n";
        return;
    }
    m_groundRequest = std::make_unique<GSReqGroundScan>();
    m_simConnect.RequestObjectDataByType(kGroundDefinition, 5'000,
                                         SIMCONNECT_SIMOBJECT_TYPE_GROUND,
                                         *m_groundRequest);
    std::cout << "Requested the current 5 km ground-object debug snapshot.\n";
}

void ConsoleController::PollGroundDebugSnapshot()
{
    CollectFinishedSetupRequests();
    if (!m_groundRequest || !m_groundRequest->IsFinished()) return;

    GSGroundScanResult result = m_groundRequest->TakeResult();
    m_groundRequest.reset();
    if (!result.succeeded) {
        std::cout << "Ground-object debug request failed or timed out.\n";
        PrintPrompt();
        return;
    }
    m_lastGroundObjects = std::move(result.objects);
    PrintGround(m_lastGroundObjects);
    PrintPrompt();
}
void ConsoleController::RequestRoads(const std::string &icao)
{
    if (icao.empty()) {
        std::cout << "Usage: roads <ICAO>  (e.g. roads KJFK)\n";
        PrintPrompt();
        return;
    }
    if (m_roadsRequest) {
        std::cout << "A roads request is already in progress.\n";
        PrintPrompt();
        return;
    }
    m_roadsRequest = std::make_unique<GSReqRoads>();
    m_roadsRequest->Drive(m_simConnect, icao);
    std::cout << "Requesting facility data for roads at " << icao << "...\n";
}

void ConsoleController::PollRoadsSnapshot()
{
    if (!m_roadsRequest || !m_roadsRequest->IsFinished()) return;

    GSRoadsResult result = m_roadsRequest->TakeResult();
    m_roadsRequest.reset();
    if (!result.succeeded) {
        std::cout << "Roads request failed or timed out.\n";
        PrintPrompt();
        return;
    }
    PrintRoads(result);
    PrintPrompt();
}

void ConsoleController::PrintRoads(const GSRoadsResult &roads)
{
    if (roads.hasAirport) {
        std::cout << "ARP: " << std::fixed << std::setprecision(5)
                  << roads.airportLatitude << ", " << roads.airportLongitude << " ("
                  << std::setprecision(1) << roads.airportAltitudeMeters << "m)"
                  << "  taxi points: " << roads.taxiPoints << ", paths: " << roads.taxiPaths
                  << ", non-aircraft roads: " << roads.roads.size()
                  << ", other paths: " << roads.otherPathCount << '\n';
    }
    if (roads.roads.empty()) {
        std::cout << "No non-aircraft roads (TAXI_PATH TYPE 6/7) were returned.\n";
        return;
    }
    for (const auto &road : roads.roads) {
        std::cout << "road type=" << road.type
                  << " width=" << std::fixed << std::setprecision(1) << road.widthMeters
                  << "m startPt=" << road.startPointIndex << " endPt=" << road.endPointIndex;
        if (road.startResolved) {
            std::cout << " start=" << std::setprecision(6) << road.startLatitude << ","
                      << road.startLongitude;
        }
        if (road.endResolved) {
            std::cout << " end=" << std::setprecision(6) << road.endLatitude << ","
                      << road.endLongitude;
        }
        std::cout << '\n';
    }
}


GSReqCommand &ConsoleController::NewSetupRequest()
{
    auto request = std::make_unique<GSReqCommand>();
    GSReqCommand &reference = *request;
    std::scoped_lock lock(m_setupRequestMutex);
    m_setupRequests.push_back(std::move(request));
    return reference;
}

void ConsoleController::CollectFinishedSetupRequests()
{
    std::scoped_lock lock(m_setupRequestMutex);
    std::erase_if(m_setupRequests, [](const auto &request) {
        return request->IsFinished();
    });
}

std::size_t ConsoleController::LastGroundCount() const
{
    return m_lastGroundObjects.size();
}

void ConsoleController::Reset()
{
    m_lastGroundObjects.clear();
}
} // namespace parking_services
