#include "GSAircraft.h"
#include "GSSimConnect.h"
#include "GSDefinitions.h"
#include "GSGeography.h"
#include "GSLogStream.h"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace NS_GSLiveAirportMSFS
{


constexpr std::array GSDatums_Aircraft{
    DatumSpec{"ATC ID", "", SIMCONNECT_DATATYPE_STRING32},
    DatumSpec{"ATC AIRLINE", "", SIMCONNECT_DATATYPE_STRING32},
    DatumSpec{"ATC FLIGHT NUMBER", "", SIMCONNECT_DATATYPE_STRING32},
    DatumSpec{"PLANE LATITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64},
    DatumSpec{"PLANE LONGITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64},
    DatumSpec{"PLANE ALTITUDE", "feet", SIMCONNECT_DATATYPE_FLOAT32},
    DatumSpec{"GROUND ALTITUDE", "feet", SIMCONNECT_DATATYPE_FLOAT32},
    DatumSpec{"PLANE HEADING DEGREES TRUE", "degrees", SIMCONNECT_DATATYPE_FLOAT32},
    DatumSpec{"GROUND VELOCITY", "knots", SIMCONNECT_DATATYPE_FLOAT32},
    DatumSpec{"AI TRAFFIC CURRENT AIRPORT", "", SIMCONNECT_DATATYPE_STRING8},
    DatumSpec{"AI TRAFFIC ASSIGNED RUNWAY", "", SIMCONNECT_DATATYPE_STRING8},
    DatumSpec{"AI TRAFFIC FROMAIRPORT", "", SIMCONNECT_DATATYPE_STRING8},
    DatumSpec{"AI TRAFFIC TOAIRPORT", "", SIMCONNECT_DATATYPE_STRING8},
    DatumSpec{"AI TRAFFIC ASSIGNED PARKING", "", SIMCONNECT_DATATYPE_STRING32},
    DatumSpec{"AI TRAFFIC STATE", "", SIMCONNECT_DATATYPE_STRING32},
    DatumSpec{"AI TRAFFIC ETD", "seconds", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"AI TRAFFIC ETA", "seconds", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"LIGHT BEACON", "bool", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"LIGHT NAV", "bool", SIMCONNECT_DATATYPE_INT8},
    DatumSpec{"LIGHT TAXI", "bool", SIMCONNECT_DATATYPE_INT8},
    DatumSpec{"BRAKE PARKING POSITION", "bool", SIMCONNECT_DATATYPE_INT8},
    DatumSpec{"SIM ON GROUND", "bool", SIMCONNECT_DATATYPE_INT8},

    DatumSpec{"TITLE", "", SIMCONNECT_DATATYPE_STRING32},
    DatumSpec{"IS USER SIM", "bool", SIMCONNECT_DATATYPE_INT8},    
    DatumSpec{"NUMBER OF ENGINES", "number", SIMCONNECT_DATATYPE_INT8}, 
    DatumSpec{"PUSHBACK ATTACHED", "bool", SIMCONNECT_DATATYPE_INT8},
    DatumSpec{"PUSHBACK WAIT", "bool", SIMCONNECT_DATATYPE_INT8},   
    DatumSpec{"WING SPAN", "meters", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"PUSHBACK CONTACTX", "meters", SIMCONNECT_DATATYPE_FLOAT32},
    DatumSpec{"PUSHBACK CONTACTZ", "meters", SIMCONNECT_DATATYPE_FLOAT32},
    
};

constexpr std::array GSDatums_Aircraft_Dynamic{
    DatumSpec{"INTERACTIVE POINT TYPE EX1", "enum", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"INTERACTIVE POINT POSX EX1", "meters", SIMCONNECT_DATATYPE_FLOAT32},
    DatumSpec{"INTERACTIVE POINT POSY EX1", "meters", SIMCONNECT_DATATYPE_FLOAT32},
    DatumSpec{"INTERACTIVE POINT POSZ EX1", "meters", SIMCONNECT_DATATYPE_FLOAT32},
    DatumSpec{"INTERACTIVE POINT HEADING EX1", "degrees", SIMCONNECT_DATATYPE_FLOAT32},
};

void GSAircraft::InitDatums(GSSimConnect& handler)
{
    bool rc = true;
    for (const DatumSpec &datum : GSDatums_Aircraft) {
        rc &= handler.InvokeAddDatum(GSDefinitions::GSDefID_Aircraft, datum.name, datum.units, datum.type);
    }

    if (!rc) {
        GSLogStream::LogError("Unable to add definitions for GSAircraft");
        return;
    }

    for (std::size_t index = 0; index < GSAircraft::s_MaxInteractivePnts; ++index) {
        const std::string suffix = ":" + std::to_string(index);
        for (std::size_t dt = 0; dt < GSDatums_Aircraft_Dynamic.size(); ++dt) {
            const DatumSpec& datum = GSDatums_Aircraft_Dynamic[dt];
            rc &= handler.InvokeAddDatum(GSDefinitions::GSDefID_Aircraft, datum.name, datum.units, datum.type);
        }
    }

    if (!rc) {
        GSLogStream::LogError("Unable to add dynamic definitions for GSAircraft");
    }
}

void GSAircraft::CopyDynInfo(const GSAircraft& another)
{
    memcpy(&m_rawData, &another.m_rawData, AIRCRAFT_WIREDATA_DYNSIZE);
}

void GSAircraft::LoadDynamicState(const SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry)
{
    GSSimConnect::ReadMsgData(&m_rawData, AIRCRAFT_WIREDATA_DYNSIZE, entry);
}

void GSAircraft::LoadFullState(const SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry)
{
    GSSimConnect::ReadMsgData(&m_rawData, sizeof(m_rawData), entry);
    objectID = entry.dwObjectID;

    m_category = AircraftSizeCategory::Small;
    if (m_rawData.wingSpanMeters > 24.0) m_category = AircraftSizeCategory::Medium;
    if (m_rawData.wingSpanMeters > 36.0) m_category = AircraftSizeCategory::Large;
    if (m_rawData.wingSpanMeters > 65.0) m_category = AircraftSizeCategory::ExtraLarge;

    for (const auto& interactivePnt : m_rawData.interactivePoints) {
        if (interactivePnt.type > 4 || interactivePnt.type == 2)
            continue;

        switch (interactivePnt.type) {
        case 0: // Main
            if (interactivePnt.posZMeter < 0) {
                if (interactivePnt.posXMeter < 0)
                    rearLeftDoorPnt = std::cref(interactivePnt);
                else
                    rearRightDoorPnt = std::cref(interactivePnt);
            } else {
                if (interactivePnt.posXMeter < 0)
                    frontLeftDoorPnt = std::cref(interactivePnt);
                else
                    frontRightDoorPnt = std::cref(interactivePnt);
            }
        case 1: // Cargo
            if (interactivePnt.posZMeter < 0)
                cargoDoorBackPnt = std::cref(interactivePnt);
            else
                cargoDoorFrontPnt = std::cref(interactivePnt);
        case 3: 
            fuelHosePnt = std::cref(interactivePnt);
            break;
        case 4: // GroundPower
            groundPowerPnt = std::cref(interactivePnt);
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

double GSAircraft::LateralDistanceFrom(const GSAircraft* ac) const
{
    return GSGeography::DistanceMeters(m_rawData.latitude, m_rawData.longitude, ac->m_rawData.latitude, ac->m_rawData.longitude);
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

void GSAircraft::Print() const
{
    GSLogStream::Log() << "============================================================================" << std::endl;
    GSLogStream::Log() << "Aircraft #" << objectID << std::endl;
    GSLogStream::Log() << "  atc=" << GSLogStream::CharArrayToString(m_rawData.atcId)
            << "  airline=" << GSLogStream::CharArrayToString(m_rawData.atcAirline)
            << "  flight=" << GSLogStream::CharArrayToString(m_rawData.atcFlightNumber)
            << "  title=" << GSLogStream::CharArrayToString(m_rawData.title) << std::endl;
    GSLogStream::Log() << "  cat=" << AircraftSizeName(m_category)
            << "  lat=" << m_rawData.latitude << "  lon=" << m_rawData.longitude
            << "  altFt=" << m_rawData.altitudeFeet << "  gndAltFt=" << m_rawData.groundAltitudeFeet
            << "  hdgDeg=" << m_rawData.headingDegrees << "  gndSpdKt=" << m_rawData.groundSpeedKnots << std::endl;
    GSLogStream::Log() << "  from=" << GSLogStream::CharArrayToString(m_rawData.fromAirport)
            << "  to=" << GSLogStream::CharArrayToString(m_rawData.toAirport) 
            << "  curr=" << GSLogStream::CharArrayToString(m_rawData.currentAirport) << std::endl;
    GSLogStream::Log() << "  runway=" << GSLogStream::CharArrayToString(m_rawData.assignedRunway)
            << "  parking=" << GSLogStream::CharArrayToString(m_rawData.assignedParking)
            << "  state=" << GSLogStream::CharArrayToString(m_rawData.trafficState)
            << "  etdSec=" << m_rawData.etdSeconds << "  etaSec=" << m_rawData.etaSeconds << std::endl;
    GSLogStream::Log() << "  lights beacon=" << m_rawData.lightBeacon
            << "  nav=" << static_cast<int>(m_rawData.lightNav)
            << "  taxi=" << static_cast<int>(m_rawData.lightTaxi)
            << "  parkBrake=" << static_cast<int>(m_rawData.parkingBrake)
            << "  onGround=" << static_cast<int>(m_rawData.onGround) << std::endl;
    GSLogStream::Log() << "  wingSpanM=" << m_rawData.wingSpanMeters
            << "  engines=" << static_cast<int>(m_rawData.numberOfEngines)
            << "  pushbackAttached=" << static_cast<int>(m_rawData.pushbackAttached)
            << "  pushbackWait=" << static_cast<int>(m_rawData.pushbackWait)
            << "  pushbackContactX=" << m_rawData.pushbackContactXMeters
            << "  pushbackContactZ=" << m_rawData.pushbackContactZMeters;

    int validPoints = 0;
    std::string ipLine;
    int onLine = 0;
    int ipIndex = 0;
    for (std::size_t i = 0; i < m_rawData.interactivePoints.size(); ++i) {
        const auto &p = m_rawData.interactivePoints[i];
        if (!IsValidInteractivePointType(p.type)) {
            continue;
        }
        ++validPoints;
        std::ostringstream item;
        item << "IP" << ipIndex++ << ": \"" << InteractivePointTypeName(p.type)
             << "\", " << p.posXMeter << "," << p.posYMeter << "," << p.posZMeter
             << ", " << p.headingDegrees;
        if (onLine > 0) ipLine += "   ";
        ipLine += item.str();
        if (++onLine == 3) {
            GSLogStream::Log() << "   " << ipLine;
            ipLine.clear();
            onLine = 0;
        }
    }
    if (onLine > 0) GSLogStream::Log() << "   " << ipLine;
    GSLogStream::Log() << "   interactivePoints: " << validPoints << " valid";
}

} // namespace NS_GSLiveAirportMSFS
