#include "GSTruckFacility.h"
#include "../General/GSGeography.h"
#include "../General/GSCatalog.h"
#include "GSStandingHuman.h"

namespace NS_GSLiveAirportMSFS
{

bool GSTruckFacility::OnCreated()
{
    FinalizeRoute();
    return false;
}

void GSTruckFacility::OnArrived()
{
    Freeze();
    ContinueSpawn();
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
        airData.headingDegrees, m_aircraft.GetLongLat(), 0, halfWing + 6);
    m_initPos.Longitude = conePos.Long();
    m_initPos.Latitude = conePos.Lat();

    auto ptrH1 = new GSStandingHuman(m_simHandle, m_aircraft, *this, GSStandingHuman::PILOT,
        { halfWing + 4, 3 });
    ptrH1->SetPosition({ -halfWing / 4, -halfWing - 6 });
    ptrH1->PreSpawn();
    m_attached.emplace_back(ptrH1);

    auto ptrH2 = new GSStandingHuman(m_simHandle, m_aircraft, *this, GSStandingHuman::PASSENGER,
        { halfWing + 3, 4 });
    ptrH2->SetPosition({ halfWing / 2, -halfWing - 6 });
    ptrH2->PreSpawn();
    m_attached.emplace_back(ptrH2);

    PrepareRoute();

    return true;
}

}
