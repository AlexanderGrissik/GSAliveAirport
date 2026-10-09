// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include <cstdint>
#include <string>

namespace NS_GSAliveAirport
{
class GSAircraft;

// Serializes GSAircraft state into the JSON messages the application UI understands.
class GSAliveUIAPI
{
public:
    // Full snapshot: identity, position, route, systems, static aircraft data, interactive points.
    static std::string AddAircraft(const GSAircraft &ac, const std::string &service = "none");
    // Dynamic fields only (position, route, systems). Static fields from AddAircraft are not resent.
    static std::string UpdateAircraft(const GSAircraft &ac, const std::string &service);
    static std::string RemoveAircraft(std::uint32_t id);
    // Compact snapshot for the dedicated user-aircraft pane: id + position fields + on-ground.
    static std::string SetUserAircraft(const GSAircraft &ac);

private:

    static std::string JsonString(const std::string &value);
    static std::string CStr(const char *value);
};
} // namespace NS_GSAliveAirport