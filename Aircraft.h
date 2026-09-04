#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace parking_services
{
using AircraftId = std::uint32_t;
using TrackerClock = std::chrono::steady_clock;

struct AircraftCargoConnectionPoint
{
    double forwardMeters{};
    double rightMeters{};
    double verticalMeters{};
    double relativeHeadingDegrees{};
    std::uint32_t interactivePointIndex{};
};

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
    double wingSpanMeters{};
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
    std::optional<AircraftCargoConnectionPoint> cargoDoorRightFront;
    std::optional<AircraftCargoConnectionPoint> cargoDoorRightBack;
    // Ground Power cable receptacle (interactive point type 4); set only when
    // this airframe can accept a Ground Power Unit.
    std::optional<AircraftCargoConnectionPoint> groundPower;
    double distanceFromUserMeters{};
    TrackerClock::time_point firstSeen{};
    std::optional<TrackerClock::time_point> parkedSince;
    TrackerClock::time_point firstTracked{};
    TrackerClock::time_point lastSeen{};
};

enum class AircraftSizeCategory
{
    Small,
    Medium,
    Large,
    ExtraLarge
};

std::string NormalizeTrafficState(std::string state);
std::optional<AircraftSizeCategory> ClassifyAircraftSize(double wingSpanMeters);
std::string_view AircraftSizeCategoryName(AircraftSizeCategory category);
double MetersPerDegreeLat();
double MetersPerDegreeLong(double latitude);
double DistanceMeters(double latitudeA, double longitudeA,
                      double latitudeB, double longitudeB);
} // namespace parking_services
