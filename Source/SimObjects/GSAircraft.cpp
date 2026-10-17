// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSAircraft.h"
#include "../General/GSSimConnect.h"
#include "../General/GSDefinitions.h"
#include "../General/GSGeography.h"
#include "../General/GSLogStream.h"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace NS_GSAliveAirport
{


constexpr std::array GSDatums_Aircraft{
    GSDefinitions::DatumSpec{"ATC ID", "", SIMCONNECT_DATATYPE_STRING32},
    GSDefinitions::DatumSpec{"ATC AIRLINE", "", SIMCONNECT_DATATYPE_STRING32},
    GSDefinitions::DatumSpec{"ATC FLIGHT NUMBER", "", SIMCONNECT_DATATYPE_STRING32},
    GSDefinitions::DatumSpec{"PLANE LATITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64},
    GSDefinitions::DatumSpec{"PLANE LONGITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64},
    GSDefinitions::DatumSpec{"PLANE ALTITUDE", "feet", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"GROUND ALTITUDE", "feet", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"PLANE HEADING DEGREES TRUE", "degrees", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"PLANE ALT ABOVE GROUND", "feet", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"PLANE ALT ABOVE GROUND MINUS CG", "feet", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"GROUND VELOCITY", "knots", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"AI TRAFFIC CURRENT AIRPORT", "", SIMCONNECT_DATATYPE_STRING8},
    GSDefinitions::DatumSpec{"AI TRAFFIC FROMAIRPORT", "", SIMCONNECT_DATATYPE_STRING8},
    GSDefinitions::DatumSpec{"AI TRAFFIC TOAIRPORT", "", SIMCONNECT_DATATYPE_STRING8},
    GSDefinitions::DatumSpec{"AI TRAFFIC ASSIGNED PARKING", "", SIMCONNECT_DATATYPE_STRING32},
    GSDefinitions::DatumSpec{"AI TRAFFIC STATE", "", SIMCONNECT_DATATYPE_STRING32},
    GSDefinitions::DatumSpec{"AI TRAFFIC ETD", "seconds", SIMCONNECT_DATATYPE_INT32},
    GSDefinitions::DatumSpec{"AI TRAFFIC ETA", "seconds", SIMCONNECT_DATATYPE_INT32},
    GSDefinitions::DatumSpec{"LIGHT BEACON", "bool", SIMCONNECT_DATATYPE_INT32},
    GSDefinitions::DatumSpec{"LIGHT NAV", "bool", SIMCONNECT_DATATYPE_INT32},
    GSDefinitions::DatumSpec{"LIGHT TAXI", "bool", SIMCONNECT_DATATYPE_INT32},
    GSDefinitions::DatumSpec{"BRAKE PARKING POSITION", "bool", SIMCONNECT_DATATYPE_INT32},
    GSDefinitions::DatumSpec{"SIM ON GROUND", "bool", SIMCONNECT_DATATYPE_INT32},
    GSDefinitions::DatumSpec{"IS USER SIM", "bool", SIMCONNECT_DATATYPE_INT32},

    GSDefinitions::DatumSpec{"TITLE", "", SIMCONNECT_DATATYPE_STRING32},
    GSDefinitions::DatumSpec{"NUMBER OF ENGINES", "number", SIMCONNECT_DATATYPE_INT32},
    GSDefinitions::DatumSpec{"PUSHBACK ATTACHED", "bool", SIMCONNECT_DATATYPE_INT32},
    GSDefinitions::DatumSpec{"PUSHBACK WAIT", "bool", SIMCONNECT_DATATYPE_INT32},
    GSDefinitions::DatumSpec{"WING SPAN", "meters", SIMCONNECT_DATATYPE_FLOAT64},
    GSDefinitions::DatumSpec{"PUSHBACK CONTACTX", "meters", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"PUSHBACK CONTACTZ", "meters", SIMCONNECT_DATATYPE_FLOAT32},
};

constexpr std::array GSDatums_Aircraft_Dynamic{
    GSDefinitions::DatumSpec{"INTERACTIVE POINT TYPE EX1", "enum", SIMCONNECT_DATATYPE_INT32},
    GSDefinitions::DatumSpec{"INTERACTIVE POINT POSX EX1", "meters", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"INTERACTIVE POINT POSY EX1", "feet", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"INTERACTIVE POINT POSZ EX1", "meters", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"INTERACTIVE POINT HEADING EX1", "degrees", SIMCONNECT_DATATYPE_FLOAT32},
};

void GSAircraft::InitDatums(GSSimConnect& handler)
{
    handler.InvokeAddDatums(GSDatums_Aircraft, GSDefinitions::GSDefID_Aircraft);

    for (std::size_t index = 0; index < GSAircraft::s_MaxInteractivePnts; ++index) {
        const std::string suffix = ":" + std::to_string(index);
        for (std::size_t dt = 0; dt < GSDatums_Aircraft_Dynamic.size(); ++dt) {
            const GSDefinitions::DatumSpec& datum = GSDatums_Aircraft_Dynamic[dt];
            handler.InvokeAddDatum(GSDefinitions::GSDefID_Aircraft, (datum.name + suffix).c_str(), datum.units, datum.type);
        }
    }
}

void GSAircraft::CopyDynInfo(const GSAircraft& another)
{
    memcpy(&m_rawData, &another.m_rawData, AIRCRAFT_WIREDATA_DYNSIZE);
    m_trafficState = m_rawData.trafficState.data();
    m_parkingState = m_rawData.assignedParking.data();
}

void GSAircraft::LoadDynamicState(const SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry)
{
    GSSimConnect::ReadMsgData(&m_rawData, AIRCRAFT_WIREDATA_DYNSIZE, entry);
    objectID = entry.dwObjectID;
    m_trafficState = m_rawData.trafficState.data();
    m_parkingState = m_rawData.assignedParking.data();
}

void GSAircraft::LoadFullState(const SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry)
{
    GSSimConnect::ReadMsgData(&m_rawData, sizeof(m_rawData), entry);
    objectID = entry.dwObjectID;

    m_departing = (m_rawData.onGround && m_rawData.groundSpeedKnots < 2.0);

    m_category = AircraftSizeCategory::Small;
    if (m_rawData.wingSpanMeters > 24.0) m_category = AircraftSizeCategory::Medium;
    if (m_rawData.wingSpanMeters > 36.0) m_category = AircraftSizeCategory::Large;
    if (m_rawData.wingSpanMeters > 65.0) m_category = AircraftSizeCategory::ExtraLarge;

    
    for (size_t idx = 0; idx < m_rawData.interactivePoints.size(); ++idx) {
        const auto& interactivePnt = m_rawData.interactivePoints[idx];
        if (interactivePnt.type > 4 || interactivePnt.type == 2)
            continue;

        switch (interactivePnt.type) {
        case 0: // Main
            if (interactivePnt.posZMeter < 0) {
                if (interactivePnt.posXMeter < 0) {
                    rearLeftDoorPnt = std::make_pair(std::cref(interactivePnt), idx);
                } else {
                    rearRightDoorPnt = std::make_pair(std::cref(interactivePnt), idx);
                }
            } else {
                if (interactivePnt.posXMeter < 0) {
                    frontLeftDoorPnt = std::make_pair(std::cref(interactivePnt), idx);
                } else {
                    frontRightDoorPnt = std::make_pair(std::cref(interactivePnt), idx);
                }
            }
            break;
        case 1: // Cargo
            if (interactivePnt.posZMeter < 0) {
                cargoDoorBackPnt = std::make_pair(std::cref(interactivePnt), idx);
            }
            else {
                cargoDoorFrontPnt = std::make_pair(std::cref(interactivePnt), idx);
            }
            break;
        case 3: 
            fuelHosePnt = std::make_pair(std::cref(interactivePnt), idx);
            break;
        case 4: // GroundPower
            groundPowerPnt = std::make_pair(std::cref(interactivePnt), idx);
            break;
        }
    }   
}

bool GSAircraft::operator==(const GSAircraft& another) const
{
    if (objectID == another.objectID) {
        return (memcmp(&m_rawData, &another.m_rawData, AIRCRAFT_WIREDATA_DYNSIZE) == 0);
    }

    return false;
}

bool GSAircraft::IsParkedActive() const
{
    return (!m_rawData.isUser && m_rawData.onGround && m_rawData.lightNav &&
            std::abs(m_rawData.groundSpeedKnots) < 1.0);
}

double GSAircraft::LateralDistanceMetersFrom(const GSAircraft& ac) const
{
    return GSGeography::DistanceMeters(GetLongLat(), ac.GetLongLat());
}

const char *GSAircraft::InteractivePointTypeName(std::int32_t type)
{
    switch (type)
    {
    case 0: return "Main";
    case 1: return "Cargo";
    case 3: return "Fuel";
    case 4: return "GroundPower";
    default: return "Other";
    }
}

const char *GSAircraft::AircraftSizeName(GSAircraft::AircraftSizeCategory category)
{
    switch (category)
    {
    case AircraftSizeCategory::Small: return "Small";
    case AircraftSizeCategory::Medium: return "Medium";
    case AircraftSizeCategory::Large: return "Large";
    case AircraftSizeCategory::ExtraLarge: return "ExtraLarge";
    }
    return "Unknown";
}

} // namespace NS_GSAliveAirport
