#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace parking_services
{
using AircraftId = std::uint32_t;
using TrackerClock = std::chrono::steady_clock;

struct AircraftSnapshot
{
    AircraftId objectId{};
    std::string title;
    std::string atcId;
    std::string atcAirline;
    std::string atcFlightNumber;
    double latitude{};
    double longitude{};
    double altitudeFeet{};
    double groundAltitudeFeet{};
    double headingDegrees{};
    double groundSpeedKnots{};
    bool onGround{};
    bool isUser{};
    std::string currentAirport;
    std::string assignedParking;
    std::string assignedRunway;
    std::string fromAirport;
    std::string toAirport;
    int etdSeconds{};
    int etaSeconds{};
    std::string trafficState;
    bool isIfr{};
    int numberOfEngines{};
    std::array<int, 4> engineCombustion{};
    std::array<int, 4> engineStarterActive{};
    std::array<double, 4> engineN1Percent{};
    bool lightBeacon{};
    bool lightNav{};
    bool lightTaxi{};
    bool lightStrobe{};
    bool parkingBrake{};
    bool pushbackAttached{};
    bool pushbackWait{};
    int transponderState{};
    double distanceFromUserMeters{};
    TrackerClock::time_point firstSeen{};
    std::optional<TrackerClock::time_point> parkedSince;
    TrackerClock::time_point firstTracked{};
    TrackerClock::time_point lastSeen{};
};

std::string NormalizeTrafficState(std::string state);
double MetersPerDegree(double latitude);
double DistanceMeters(double latitudeA, double longitudeA,
                      double latitudeB, double longitudeB);
} // namespace parking_services
