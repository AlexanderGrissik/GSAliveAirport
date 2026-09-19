#include "GSSmallCones.h"
#include "../General/GSGeography.h"
#include "../General/GSCatalog.h"
#include "GSStatic.h"

namespace NS_GSLiveAirportMSFS
{

bool GSSmallCones::OnCreated()
{
    Freeze();
    return true;
}

bool GSSmallCones::PreSpawn()
{
    const auto& airData = m_aircraft.GetRawData();

    m_title = "Cone_Medium";
    m_initPos.Heading = airData.headingDegrees;
    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    m_initPos.OnGround = 1;
    m_initPos.Altitude = airData.groundAltitudeFeet;

    auto halfWing = airData.wingSpanMeters / 2.0;
    const auto conePos = GSGeography::RelativePosition(
        airData.headingDegrees, m_aircraft.GetLongLat(), 0, -halfWing);
    m_initPos.Longitude = conePos.Long();
    m_initPos.Latitude = conePos.Lat();

    auto ptrCone2 = new GSStatic(m_simHandle, m_aircraft, *this);
    ptrCone2->SetTitle("Cone_Medium");
    ptrCone2->SetPosition({ halfWing, 0 });
    ptrCone2->PreSpawn();
    m_attached.emplace_back(ptrCone2);

    return true;
}

}
