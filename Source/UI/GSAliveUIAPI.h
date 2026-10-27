// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include <cstdint>
#include <sstream>
#include <string>

namespace NS_GSAliveAirport
{
class GSAircraft;
class GSAirport;

// Serializes GSAircraft state into the JSON messages the application UI understands.
class GSAliveUIAPI
{
public:
    // Ground-service state of a listed aircraft (internal name only — the UI renders its own text/colors).
    enum class AircraftService : std::uint8_t
    {
        None = 0,
        Spawning,
        Inplace,
    };

    // Full snapshot: identity, position, route, systems, static aircraft data, interactive points.
    // Service always starts as None.
    static std::string AddAircraft(const GSAircraft &ac);
    // Dynamic fields only (position, route, systems). Does not alter the service state.
    static std::string UpdateAircraft(const GSAircraft &ac);
    // Service state only — changes nothing else.
    static std::string UpdateAircraftServices(std::uint32_t id, AircraftService service);
    static std::string RemoveAircraft(std::uint32_t id);
    // Compact snapshot for the dedicated user-aircraft pane: id + position fields + current airport + on-ground.
    static std::string SetUserAircraft(const GSAircraft& ac);
    // Global status for the top-bar title: true = connected (green), false = waiting (yellow).
    static std::string SetGlobalStatus(bool isConnected);

private:

    static std::string JsonString(const std::string &value);
    // Appends the dynamic aircraft fields (position/route/systems/parking) shared by add and update.
    static void AircraftDynamicInfo(const GSAircraft &ac, std::ostringstream& stream);
};
} // namespace NS_GSAliveAirport
