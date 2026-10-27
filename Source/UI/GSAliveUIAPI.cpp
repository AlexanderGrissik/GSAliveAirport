// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSAliveUIAPI.h"

#include "../General/GSAirport.h"
#include "../SimObjects/GSAircraft.h"

#include <cstddef>
#include <sstream>

namespace NS_GSAliveAirport
{
std::string GSAliveUIAPI::JsonString(const std::string &value)
{
    std::string result = "\"";
    for (char c : value)
    {
        switch (c)
        {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                result += buffer;
            }
            else
            {
                result.push_back(c);
            }
            break;
        }
    }
    result.push_back('"');
    return result;
}

std::string GSAliveUIAPI::AddAircraft(const GSAircraft &ac)
{
    std::ostringstream stream;
    stream << "{\"op\":\"aircraft.add\",\"id\":" << ac.GetObjID();
    stream << ",\"atc\":" << JsonString(ac.GetATCID())
           << ",\"airline\":" << JsonString(ac.GetATCAirline())
           << ",\"flight\":" << JsonString(ac.GetARCFlightNumber());
    stream << ",\"title\":" << JsonString(ac.GetTitle())
           << ",\"category\":" << JsonString(GSAircraft::AircraftSizeName(ac.GetCategory()));
    AircraftDynamicInfo(ac, stream);
    stream << ",\"aiParking\":" << JsonString(ac.GetAssignedParking())
           << ",\"departing\":" << (ac.IsDeparting() ? "true" : "false");
    stream << ",\"wingSpanM\":" << ac.GetWingSpanMeters() << ",\"engines\":" << ac.GetNumberOfEngines();
    stream << ",\"pushX\":" << ac.GetPushbackContactXMeters() << ",\"pushZ\":" << ac.GetPushbackContactZMeters()
           << ",\"service\":" << static_cast<unsigned int>(AircraftService::None);
    stream << ",\"interactivePoints\":[";
    bool first = true;
    for (const auto &p : ac.GetInteractivePoints()) {
        if (!GSAircraft::IsValidInteractivePointType(p.type)) continue;
        if (p.posXMeter == 0.0f && p.posYFeet == 0.0f && p.posZMeter == 0.0f) continue;
        if (!first) stream << ",";
        first = false;
        stream << "{\"type\":" << JsonString(GSAircraft::InteractivePointTypeName(p.type))
               << ",\"x\":" << p.posXMeter << ",\"y\":" << p.posYFeet << ",\"z\":" << p.posZMeter << ",\"hdg\":" << p.headingDegrees << "}";
    }
    stream << "]}";
    return stream.str();
}

void GSAliveUIAPI::AircraftDynamicInfo(const GSAircraft &ac, std::ostringstream& stream)
{
    const auto* parking = ac.GetParking();
    stream << ",\"lat\":" << ac.GetLongLat().Lat() << ",\"lon\":" << ac.GetLongLat().Long()
           << ",\"altFt\":" << ac.GetAltitudeFeet() << ",\"gndAltFt\":" << ac.GetGroundAltitudeFeet();
    stream << ",\"hdgDeg\":" << ac.GetHeading() << ",\"gndSpdKt\":" << ac.GetGroundSpeedKnots();
    stream << ",\"from\":" << JsonString(ac.GetFromAirport())
           << ",\"to\":" << JsonString(ac.GetToAirport())
           << ",\"curr\":" << JsonString(ac.GetAICurrAirport() + "/" + ac.GetAirport()->GetICAO());
    stream << ",\"state\":" << JsonString(ac.GetTrafficState())
           << ",\"etdSec\":" << ac.GetETDSeconds() << ",\"etaSec\":" << ac.GetETASeconds();
    stream << ",\"beacon\":" << ac.IsLightBeaconOn() << ",\"nav\":" << ac.IsLightNavOn() << ",\"taxi\":" << ac.IsLightTaxiOn()
           << ",\"parkBrake\":" << ac.IsParkingBrakeSet() << ",\"onGround\":" << (ac.IsOnGround() ? "true" : "false");
    stream << ",\"pushbackAttached\":" << ac.IsPushbackAttached() << ",\"pushbackWait\":" << ac.IsPushbackWait();
    stream << ",\"parking\":" << (parking != nullptr ? std::to_string(parking->m_parkingNumber) : "null")
           << ",\"jetway\":" << (parking != nullptr && parking->m_hasJetway ? "true" : "false");
}

std::string GSAliveUIAPI::UpdateAircraft(const GSAircraft &ac)
{
    std::ostringstream stream;
    stream << "{\"op\":\"aircraft.update\",\"id\":" << ac.GetObjID();
    AircraftDynamicInfo(ac, stream);
    stream << "}";
    return stream.str();
}

std::string GSAliveUIAPI::SetUserAircraft(const GSAircraft& ac)
{
    std::ostringstream stream;
    stream << "{\"op\":\"user.set\",\"id\":" << ac.GetObjID();
    stream << ",\"lat\":" << ac.GetLongLat().Lat() << ",\"lon\":" << ac.GetLongLat().Long()
           << ",\"altFt\":" << ac.GetAltitudeFeet() << ",\"gndAltFt\":" << ac.GetGroundAltitudeFeet();
    stream << ",\"hdgDeg\":" << ac.GetHeading() << ",\"gndSpdKt\":" << ac.GetGroundSpeedKnots();
    stream << (",\"curr\":" + JsonString(ac.GetAICurrAirport() + "/" + ac.GetAirport()->GetICAO()));
    stream << ",\"onGround\":" << (ac.IsOnGround() ? "true" : "false") << "}";
    return stream.str();
}

std::string GSAliveUIAPI::SetGlobalStatus(bool isConnected)
{
    return std::string("{\"op\":\"status.set\",\"connected\":") + (isConnected ? "true" : "false") + "}";
}

std::string GSAliveUIAPI::RemoveAircraft(std::uint32_t id)
{
    return std::string("{\"op\":\"aircraft.remove\",\"id\":") + std::to_string(id) + "}";
}

std::string GSAliveUIAPI::UpdateAircraftServices(std::uint32_t id, AircraftService service)
{
    return std::string("{\"op\":\"aircraft.services\",\"id\":") + std::to_string(id)
        + ",\"service\":" + std::to_string(static_cast<std::uint8_t>(service)) + "}";
}
} // namespace NS_GSAliveAirport
