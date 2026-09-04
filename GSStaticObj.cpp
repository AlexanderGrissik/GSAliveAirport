#include "GSStaticObj.h"

#include "GSCommon.h"
#include "ISimConnectHandler.h"

#include <utility>

namespace parking_services
{
GSStaticObj::GSStaticObj(AircraftSnapshot aircraft, GroundServiceObject object,
                         GroundServiceLocation location)
    : GSObject(std::move(aircraft), std::move(object), std::move(location))
{
}

bool GSStaticObj::PreparePlacement(GSObjectServices &)
{
    if (m_location.kind != GroundServiceLocationKind::Static) {
        GSLog("Skipped " + m_object.family + " for aircraft " +
              std::to_string(m_aircraft.objectId) +
              ": a static ground-service object requires a static location.");
        return false;
    }
    if (!LocationRelationAvailable()) return false;
    m_pose = RelativeToAircraft(m_aircraft, m_location.relX1, m_location.relY1,
                                m_location.faceAircraft,
                                m_location.faceAircraftReverse,
                                m_location.wingRelative,
                                m_location.relation);
    return true;
}

void GSStaticObj::ConfigureSimConnect(GSObjectServices &services)
{
    m_positionDefinition = services.allocateDataDefinition();
    m_freezeLatitudeLongitudeEvent = services.allocateClientEvent();
    m_freezeAltitudeEvent = services.allocateClientEvent();
    m_freezeAttitudeEvent = services.allocateClientEvent();

    ISimConnectHandler &simConnect = *services.simConnect;
    simConnect.AddDatum(m_positionDefinition, "Initial Position", "",
                        SIMCONNECT_DATATYPE_INITPOSITION,
                        NewCommandRequest());
    simConnect.MapClientEvent(m_freezeLatitudeLongitudeEvent,
                              "FREEZE_LATITUDE_LONGITUDE_SET",
                              NewCommandRequest());
    simConnect.MapClientEvent(m_freezeAltitudeEvent, "FREEZE_ALTITUDE_SET",
                              NewCommandRequest());
    simConnect.MapClientEvent(m_freezeAttitudeEvent, "FREEZE_ATTITUDE_SET",
                              NewCommandRequest());
}

void GSStaticObj::Activate(GSObjectServices &services, AircraftId objectId)
{
    ISimConnectHandler &simConnect = *services.simConnect;
    SetPosition(services);
    simConnect.TransmitEvent(objectId, m_freezeLatitudeLongitudeEvent, 1,
                             NewCommandRequest());
    simConnect.TransmitEvent(objectId, m_freezeAltitudeEvent, 1,
                             NewCommandRequest());
    simConnect.TransmitEvent(objectId, m_freezeAttitudeEvent, 1,
                             NewCommandRequest());
    Finish(services, m_pose);
}

void GSStaticObj::SetPosition(GSObjectServices &services)
{
    const SIMCONNECT_DATA_INITPOSITION position = ToInitialPosition(m_pose);
    services.simConnect->SetObjectData(
        m_positionDefinition, m_objectId, 0, sizeof(position), &position,
        NewCommandRequest());
}
} // namespace parking_services
