#include "GSCateringDefault.h"

#include "GSCommon.h"
#include "ISimConnectHandler.h"

#include <cstdint>
#include <utility>

namespace parking_services
{
GSCateringDefault::GSCateringDefault(AircraftSnapshot aircraft,
                                     GroundServiceObject object,
                                     GroundServiceLocation location)
    : GSObject(std::move(aircraft), std::move(object), std::move(location))
{
}

const AircraftCargoConnectionPoint *GSCateringDefault::SelectRearRightDoor(
    const std::vector<AircraftCargoConnectionPoint> &exits)
{
    const AircraftCargoConnectionPoint *best = nullptr;
    for (const auto &exit : exits) {
        if (exit.rightMeters <= 0.0) continue;
        if (!best || exit.forwardMeters < best->forwardMeters) {
            best = &exit;
        }
    }
    return best;
}

bool GSCateringDefault::PreparePlacement(GSObjectServices &)
{
    const AircraftCargoConnectionPoint *door =
        SelectRearRightDoor(m_aircraft.mainExits);
    if (!door) {
        GSLog("Skipped " + m_object.family + " for aircraft " +
              std::to_string(m_aircraft.objectId) +
              ": MSFS reported no rear-right passenger (main exit) door.");
        return false;
    }
    m_door = *door;
    m_pose = RelativeToAircraft(m_aircraft, door->rightMeters, door->forwardMeters,
                                false);
    m_pose.headingDegrees =
        NormalizeDegrees(m_aircraft.headingDegrees +
                         door->relativeHeadingDegrees + 180.0);
    m_doorSillMeters =
        (m_aircraft.altitudeFeet - m_aircraft.groundAltitudeFeet) * kFeetToMeters +
        door->verticalMeters;
    return true;
}

void GSCateringDefault::ConfigureSimConnect(GSObjectServices &services)
{
    m_positionDefinition = services.allocateDataDefinition();
    m_elevationTargetDefinition = services.allocateDataDefinition();
    m_openingTargetDefinition = services.allocateDataDefinition();
    m_freezeLatitudeLongitudeEvent = services.allocateClientEvent();
    m_freezeAltitudeEvent = services.allocateClientEvent();
    m_freezeAttitudeEvent = services.allocateClientEvent();
    m_openAircraftDoorsEvent = services.allocateClientEvent();
    m_closeAircraftDoorsEvent = services.allocateClientEvent();

    ISimConnectHandler &simConnect = *services.simConnect;
    simConnect.AddDatum(m_positionDefinition, "Initial Position", "",
                        SIMCONNECT_DATATYPE_INITPOSITION, NewCommandRequest());
    simConnect.AddDatum(m_elevationTargetDefinition, "CATERINGTRUCK ELEVATION TARGET",
                        "meters", SIMCONNECT_DATATYPE_FLOAT64, NewCommandRequest());
    simConnect.AddDatum(m_openingTargetDefinition, "CATERINGTRUCK OPENING TARGET",
                        "percent", SIMCONNECT_DATATYPE_INT32, NewCommandRequest());
    simConnect.MapClientEvent(m_freezeLatitudeLongitudeEvent,
                              "FREEZE_LATITUDE_LONGITUDE_SET", NewCommandRequest());
    simConnect.MapClientEvent(m_freezeAltitudeEvent, "FREEZE_ALTITUDE_SET",
                              NewCommandRequest());
    simConnect.MapClientEvent(m_freezeAttitudeEvent, "FREEZE_ATTITUDE_SET",
                              NewCommandRequest());
    simConnect.MapClientEvent(m_openAircraftDoorsEvent, "OPEN AIRCRAFT DOORS",
                              NewCommandRequest());
    simConnect.MapClientEvent(m_closeAircraftDoorsEvent, "CLOSE AIRCRAFT DOORS",
                              NewCommandRequest());
}

void GSCateringDefault::Activate(GSObjectServices &services, AircraftId objectId)
{
    if (services.simConnect) {
        FreezeObject(services, objectId);
        SetPosition(services, objectId);
        SetElevation(services, objectId, m_doorSillMeters);
        SetOpening(services, objectId, true);
        SetAircraftDoor(services, true);
    }
    Finish(services, m_pose);
}

void GSCateringDefault::FreezeObject(GSObjectServices &services, AircraftId objectId)
{
    services.simConnect->TransmitEvent(objectId, m_freezeLatitudeLongitudeEvent, 1,
                                       NewCommandRequest());
    services.simConnect->TransmitEvent(objectId, m_freezeAltitudeEvent, 1,
                                       NewCommandRequest());
    services.simConnect->TransmitEvent(objectId, m_freezeAttitudeEvent, 1,
                                       NewCommandRequest());
}

void GSCateringDefault::SetPosition(GSObjectServices &services, AircraftId objectId)
{
    const SIMCONNECT_DATA_INITPOSITION position = ToInitialPosition(m_pose);
    services.simConnect->SetObjectData(m_positionDefinition, objectId, 0,
                                       sizeof(position), &position,
                                       NewCommandRequest());
}

void GSCateringDefault::SetElevation(GSObjectServices &services, AircraftId objectId,
                                     double meters)
{
    services.simConnect->SetObjectData(m_elevationTargetDefinition, objectId, 0,
                                       sizeof(meters), &meters, NewCommandRequest());
}

void GSCateringDefault::SetOpening(GSObjectServices &services, AircraftId objectId,
                                   bool open)
{
    const std::int32_t value = open ? 1 : 0;
    services.simConnect->SetObjectData(m_openingTargetDefinition, objectId, 0,
                                       sizeof(value), &value, NewCommandRequest());
}

void GSCateringDefault::SetAircraftDoor(GSObjectServices &services, bool open)
{
    if (!m_door) return;
    services.simConnect->TransmitEventEx1(
        m_aircraft.objectId,
        open ? m_openAircraftDoorsEvent : m_closeAircraftDoorsEvent,
        m_door->interactivePointIndex + 1, 1, NewCommandRequest());
}

void GSCateringDefault::OnRemoved(GSObjectServices &services)
{
    if (!services.simConnect) return;
    SetOpening(services, m_objectId, false);
    SetElevation(services, m_objectId, 0.0);
    SetAircraftDoor(services, false);
}
} // namespace parking_services