// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSAliveUIAPI.h"

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


std::string GSAliveUIAPI::CStr(const char *value)
{
    std::size_t len = 0;
    while (value[len] != '\0') ++len;
    return std::string(value, len);
}

std::string GSAliveUIAPI::AddAircraft(const GSAircraft &ac, const std::string &service)
{
    const GSAircraft::AircraftWireData &d = ac.GetRawData();
    std::ostringstream stream;
    stream << "{\"op\":\"aircraft.add\",\"id\":" << ac.GetObjID();
    stream << ",\"atc\":" << JsonString(CStr(d.atcId.data()))
        << ",\"airline\":" << JsonString(CStr(d.atcAirline.data()))
        << ",\"flight\":" << JsonString(CStr(d.atcFlightNumber.data()));
    stream << ",\"title\":" << JsonString(CStr(d.title.data()))
        << ",\"category\":" << JsonString(GSAircraft::AircraftSizeName(ac.GetCategory()));
    stream << ",\"lat\":" << d.latitude << ",\"lon\":" << d.longitude
        << ",\"altFt\":" << d.altitudeFeet << ",\"gndAltFt\":" << d.groundAltitudeFeet;
    stream << ",\"hdgDeg\":" << d.headingDegrees << ",\"gndSpdKt\":" << d.groundSpeedKnots;
    stream << ",\"from\":" << JsonString(CStr(d.fromAirport.data()))
        << ",\"to\":" << JsonString(CStr(d.toAirport.data()))
        << ",\"curr\":" << JsonString(CStr(d.currentAirport.data()));
    stream << ",\"parking\":" << JsonString(CStr(d.assignedParking.data()))
        << ",\"state\":" << JsonString(CStr(d.trafficState.data()))
        << ",\"etdSec\":" << d.etdSeconds << ",\"etaSec\":" << d.etaSeconds;
    stream << ",\"beacon\":" << d.lightBeacon << ",\"nav\":" << d.lightNav << ",\"taxi\":" << d.lightTaxi
        << ",\"parkBrake\":" << d.parkingBrake << ",\"onGround\":" << d.onGround;
    stream << ",\"wingSpanM\":" << d.wingSpanMeters << ",\"engines\":" << d.numberOfEngines
        << ",\"pushbackAttached\":" << d.pushbackAttached << ",\"pushbackWait\":" << d.pushbackWait;
    stream << ",\"pushX\":" << d.pushbackContactXMeters << ",\"pushZ\":" << d.pushbackContactZMeters
        << ",\"service\":" << JsonString(service);
    stream << ",\"interactivePoints\":[";
    bool first = true;
    for (const auto &p : d.interactivePoints)
    {
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

std::string GSAliveUIAPI::UpdateAircraft(const GSAircraft &ac, const std::string &service)
{
    const GSAircraft::AircraftWireData &d = ac.GetRawData();
    std::ostringstream stream;
    stream << "{\"op\":\"aircraft.update\",\"id\":" << ac.GetObjID();
    stream << ",\"lat\":" << d.latitude << ",\"lon\":" << d.longitude
        << ",\"altFt\":" << d.altitudeFeet << ",\"gndAltFt\":" << d.groundAltitudeFeet;
    stream << ",\"hdgDeg\":" << d.headingDegrees << ",\"gndSpdKt\":" << d.groundSpeedKnots;
    stream << ",\"from\":" << JsonString(CStr(d.fromAirport.data()))
        << ",\"to\":" << JsonString(CStr(d.toAirport.data()))
        << ",\"curr\":" << JsonString(CStr(d.currentAirport.data()));
    stream << ",\"parking\":" << JsonString(CStr(d.assignedParking.data()))
        << ",\"state\":" << JsonString(CStr(d.trafficState.data()))
        << ",\"etdSec\":" << d.etdSeconds << ",\"etaSec\":" << d.etaSeconds;
    stream << ",\"beacon\":" << d.lightBeacon << ",\"nav\":" << d.lightNav << ",\"taxi\":" << d.lightTaxi
        << ",\"parkBrake\":" << d.parkingBrake << ",\"onGround\":" << d.onGround;
    stream << ",\"pushbackAttached\":" << d.pushbackAttached << ",\"pushbackWait\":" << d.pushbackWait
        << ",\"service\":" << JsonString(service) << "}";
    return stream.str();
}

std::string GSAliveUIAPI::RemoveAircraft(std::uint32_t id)
{
    return std::string("{\"op\":\"aircraft.remove\",\"id\":") + std::to_string(id) + "}";
}
} // namespace NS_GSAliveAirport