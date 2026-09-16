#include "GSSmallCones.h"
#include "../GSGeography.h"
#include "../GSCatalog.h"
#include "GSStatic.h"

namespace NS_GSLiveAirportMSFS
{

void GSSmallCones::OnCreated()
{
    SetFinalPositionAndState();
    if (m_attached.size())
        SpawnAttached();
    else
        OnObjSpawned(true);
}

void GSSmallCones::OnSpawned(bool ok, GSSimObj& obj)
{
    (void)obj; (void)ok;
    if (++m_spawned == m_attached.size())
        OnObjSpawned(true);
}

void GSSmallCones::OnDespawned(bool ok, GSSimObj& obj)
{
    (void)obj; (void)ok;
    if (--m_spawned <= 0) {
        m_attached.clear();
        Despawn();
    }
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

void GSSmallCones::SetFinalPositionAndState()
{
    Freeze();
}
}
