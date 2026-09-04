#include "GSPowerGround.h"

#include "GSCommon.h"
#include "ISimConnectHandler.h"

#include <cstdint>
#include <utility>

namespace parking_services
{
GSPowerGround::GSPowerGround(AircraftSnapshot aircraft, GroundServiceObject object,
                             GroundServiceLocation location)
    : GSObject(std::move(aircraft), std::move(object), std::move(location))
{
}

bool GSPowerGround::PreparePlacement(GSObjectServices &)
{
    if (m_location.kind != GroundServiceLocationKind::Static) {
        GSLog("Skipped " + m_object.family + " for aircraft " +
              std::to_string(m_aircraft.objectId) +
              ": a ground power unit requires a static location.");
        return false;
    }
    m_pose = RelativeToAircraft(m_aircraft, m_location.relX1, m_location.relY1,
                                m_location.faceAircraft,
                                m_location.faceAircraftReverse,
                                m_location.wingRelative);
    return true;
}

void GSPowerGround::ConfigureSimConnect(GSObjectServices &services)
{
    m_positionDefinition = services.allocateDataDefinition();
    m_hoseDeployedDefinition = services.allocateDataDefinition();
    m_freezeLatitudeLongitudeEvent = services.allocateClientEvent();
    m_freezeAltitudeEvent = services.allocateClientEvent();
    m_freezeAttitudeEvent = services.allocateClientEvent();

    ISimConnectHandler &simConnect = *services.simConnect;
    simConnect.AddDatum(m_positionDefinition, "Initial Position", "",
                        SIMCONNECT_DATATYPE_INITPOSITION,
                        NewCommandRequest());
    simConnect.AddDatum(m_hoseDeployedDefinition, "GROUNDPOWERUNIT HOSE DEPLOYED",
                        "", SIMCONNECT_DATATYPE_INT32, NewCommandRequest());
    simConnect.MapClientEvent(m_freezeLatitudeLongitudeEvent,
                              "FREEZE_LATITUDE_LONGITUDE_SET",
                              NewCommandRequest());
    simConnect.MapClientEvent(m_freezeAltitudeEvent, "FREEZE_ALTITUDE_SET",
                              NewCommandRequest());
    simConnect.MapClientEvent(m_freezeAttitudeEvent, "FREEZE_ATTITUDE_SET",
                              NewCommandRequest());
}

void GSPowerGround::Activate(GSObjectServices &services, AircraftId objectId)
{
    ISimConnectHandler &simConnect = *services.simConnect;
    SetPosition(services);
    simConnect.TransmitEvent(objectId, m_freezeLatitudeLongitudeEvent, 1,
                             NewCommandRequest());
    simConnect.TransmitEvent(objectId, m_freezeAltitudeEvent, 1,
                             NewCommandRequest());
    simConnect.TransmitEvent(objectId, m_freezeAttitudeEvent, 1,
                             NewCommandRequest());
    if (HasAircraftGroundPower()) {
        SetHoseDeployed(services, objectId, true);
    } else {
        GSLog("Ground power unit for aircraft " +
              std::to_string(m_aircraft.objectId) +
              " has no ground power receptacle; leaving the hose stowed.");
    }
    Finish(services, m_pose);
}

bool GSPowerGround::RepositionRelative(GSObjectServices &services, double x,
                                       double y, double z,
                                       double headingDegrees)
{
    m_object.parentX = x;
    m_object.parentY = y;
    m_object.parentZ = z;
    m_object.parentHeadingDegrees = headingDegrees;

    const GSObjectPos parentPose = m_parent
        ? m_parent->Pose()
        : GSObjectPos{m_aircraft.latitude, m_aircraft.longitude,
                      m_aircraft.groundAltitudeFeet,
                      m_aircraft.headingDegrees, true};
    ApplyParentPose(services, parentPose);
    return true;
}

void GSPowerGround::OnParentRepositioned(GSObjectServices &services,
                                         const GSObjectPos &parentPose)
{
    ApplyParentPose(services, parentPose);
}

void GSPowerGround::ApplyParentPose(GSObjectServices &services,
                                    const GSObjectPos &parentPose)
{
    m_pose = RelativeToParent(parentPose, m_object);
    if (m_objectId != 0) SetPosition(services);
    RepositionChildren(services);
}

void GSPowerGround::SetPosition(GSObjectServices &services)
{
    const SIMCONNECT_DATA_INITPOSITION position = ToInitialPosition(m_pose);
    services.simConnect->SetObjectData(
        m_positionDefinition, m_objectId, 0, sizeof(position), &position,
        NewCommandRequest());
}

bool GSPowerGround::HasAircraftGroundPower() const
{
    return m_aircraft.groundPower.has_value();
}

void GSPowerGround::SetHoseDeployed(GSObjectServices &services, AircraftId objectId,
                                    bool deployed)
{
    if (!services.simConnect) return;
    const std::int32_t value = deployed ? 1 : 0;
    services.simConnect->SetObjectData(m_hoseDeployedDefinition, objectId, 0,
                                       sizeof(value), &value,
                                       NewCommandRequest());
}
} // namespace parking_services
