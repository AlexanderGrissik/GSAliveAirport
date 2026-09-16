#include "GSTruckFacility.h"
#include "../GSGeography.h"
#include "../GSCatalog.h"

namespace NS_GSLiveAirportMSFS
{

void GSTruckFacility::OnCreated()
{
    SetFinalPositionAndState();
    OnObjSpawned(true);
}

bool GSTruckFacility::PreSpawn()
{
    const auto& airData = m_aircraft.GetRawData();

    m_title = GSCatalog::GetInstance().GetTruckFacility();
    m_initPos.Heading = airData.headingDegrees;
    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    m_initPos.OnGround = 1;
    m_initPos.Altitude = airData.groundAltitudeFeet;

    auto halfWing = airData.wingSpanMeters / 2.0;
    const auto conePos = GSGeography::RelativePosition(
        airData.headingDegrees, m_aircraft.GetLongLat(), 0, halfWing / 2 + 8);
    m_initPos.Longitude = conePos.Long();
    m_initPos.Latitude = conePos.Lat();

    return true;
}

void GSTruckFacility::SetFinalPositionAndState()
{
    Freeze();
}
}
