#include "Aircraft.h"
#include "SimConnectHandler.h"
#include "GSDefinitions.h"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace parking_services
{

constexpr std::size_t g_InteractivePointProbeCount = 12;

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

void GSAircraft::InitDatums(SimConnectHandler& handler)
{
    bool rc = true;
    for (const DatumSpec &datum : GSDatums_Aircraft) {
        rc &=InvokeAddDatum(GSDefinitions::GSDefID_Aircraft, datum.name, datum.units, datum.type);
    }

    if (!rc) {
        GSLogError("Unable to add definitions for GSAircraft")
        return;
    }

    for (std::size_t index = 0; index < g_InteractivePointProbeCount; ++index) {
        const std::string suffix = ":" + std::to_string(index);
        for (std::size_t dt = 0; dt < GSDatums_Aircraft_Dynamic.size(); ++dt) {
            const DatumSpec& datum = GSDatums_Aircraft_Dynamic[dt];
            rc &=InvokeAddDatum(GSDefinitions::GSDefID_Aircraft, spec.name, spec.units, spec.type);
        }
    }

    if (!rc) {
        GSLogError("Unable to add dynamic definitions for GSAircraft")
    }
}

std::optional<AircraftSizeCategory> ClassifyAircraftSize(double wingSpanMeters)
{
    
}

void GSAircraft::CopyDynInfo(const GSAircraft& another)
{
    memcpy(&m_rawData, &another.m_rawData, AIRCRAFT_WIREDATA_DYNSIZE);
}

void GSAircraft::LoadDynamicState(SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry)
{
    SimConnectHandler::ReadMsgData(&m_rawData, AIRCRAFT_WIREDATA_DYNSIZE, entry);
}

void GSAircraft::LoadFullState(SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry)
{
    SimConnectHandler::ReadMsgData(&m_rawData, sizeof(m_rawData), entry);
    objectID = entry.dwObjectID;

    m_category = AircraftSizeCategory::Small
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

bool GSAircraft::operator==(const GSAircraft& another)
{
    if (objectID == another.objectID) {
        return (memcmp(&m_rawData, &another.m_rawData, AIRCRAFT_WIREDATA_DYNSIZE) == 0);
    }

    return false;
}

} // namespace parking_services
