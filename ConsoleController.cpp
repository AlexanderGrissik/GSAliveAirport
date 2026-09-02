#include "ConsoleController.h"

#include <conio.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <utility>

namespace parking_services
{
namespace
{
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
            if (_kbhit()) static_cast<void>(_getch());
            continue;
        }
        if (key == '\r') {
            std::cout << '\n';
            const std::string line = std::exchange(m_line, {});
            handler(Parse(line));
        } else if (key == '\b') {
            if (!m_line.empty()) {
                m_line.pop_back();
                std::cout << "\b \b" << std::flush;
            }
        } else if (key >= 32 && key <= 126) {
            m_line.push_back(static_cast<char>(key));
            std::cout << static_cast<char>(key) << std::flush;
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
    if (command == "stopprobe") return {AppCommandType::StopProbe};
    if (command == "reset") return {AppCommandType::Reset};
    if (command == "catalog") return {AppCommandType::Catalog};
    if (command == "animprobe") {
        std::uint64_t objectId = 0;
        if (!(input >> objectId) || objectId == 0 ||
            objectId > (std::numeric_limits<std::uint32_t>::max)()) {
            return {AppCommandType::Unknown};
        }
        return {AppCommandType::StartProbe, static_cast<std::uint32_t>(objectId)};
    }
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
              << "  animprobe <ObjectID> Record animation data until stopprobe\n"
              << "  stopprobe           Stop animation recording\n"
              << "  catalog             Export categorized spawnable catalog\n"
              << "  reset               Clear tracking and created objects\n"
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
                  << std::setprecision(1) << data.groundSpeedKnots << "kt title="
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
                  << std::setprecision(1) << data.groundSpeedKnots << "kt\n"
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
    std::cout << "SimConnect: " << (status.connected ? "connected" : "disconnected")
              << ", tracked aircraft: " << status.trackedAircraft
              << ", nearby aircraft: " << status.nearbyAircraft
              << ", last ground debug objects: " << status.groundObjects
              << ", created services: " << status.createdServices
              << ", pending creates: " << status.pendingCreates
              << ", walking workers: " << status.walkingWorkers << '/'
              << status.maximumWalkingWorkers << ", animation probe: ";
    if (status.animationProbeActive) {
        std::cout << "ObjectID " << status.animationProbeObjectId << " ("
                  << status.animationProbeSamples << " samples)";
    } else {
        std::cout << "off";
    }
    std::cout << '\n';
}
} // namespace parking_services
