#include "GSPushback.h"
#include "../GSGeography.h"
#include "../GSCatalog.h"
#include "GSStatic.h"

namespace NS_GSLiveAirportMSFS
{

void GSPushback::OnCreated()
{
    SetFinalPositionAndState();
    if (!m_driver)
        OnObjSpawned(true);
    else
        SpawnAttached();
}

void GSPushback::OnSpawned(bool ok, GSSimObj& obj)
{
    (void)obj; (void)ok;
    OnObjSpawned(true);
}

void GSPushback::OnDespawned(bool ok, GSSimObj& obj)
{
    (void)obj; (void)ok;
    m_attached.clear();
    Despawn();
}

bool GSPushback::PreSpawn()
{
    const auto& airData = m_aircraft.GetRawData();
    m_initPos.Heading = GSGeography::NormDeg(airData.headingDegrees + 180.0);

    double zshift = 5.0;
    double xshift = 5.0;
    if (m_pushType == PUSH_SMALL) {
        m_title = "ASO_Aircraft_Caddy";
        zshift = 2;
        xshift = 1;
        m_initPos.Heading += 10.0;
    } else {
        m_title = m_pushType == PUSH_XL ? "ASO_Pushback_Blue" : "ASO_Pushback_White";
        m_initPos.Heading += 60.0;
    }

    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    m_initPos.OnGround = 1;
    m_initPos.Altitude = airData.groundAltitudeFeet;
    
    const auto doorPos = GSGeography::RelativePosition(
        airData.headingDegrees, m_aircraft.GetLongLat(), airData.wingSpanMeters / 2.0 + zshift, xshift);
    m_initPos.Longitude = doorPos.Long();
    m_initPos.Latitude = doorPos.Lat();

    if (m_driver) {
        const auto driverPos = GSGeography::RelativePosition(
            m_initPos.Heading, { m_initPos.Longitude, m_initPos.Latitude }, 1.54733, -0.35635);
        
        auto ptrDriver = new GSStatic(m_simHandle, m_aircraft, *this);
        ptrDriver->SetTitle("FSDT_Driver_01_Tug_M1A");
        ptrDriver->SetPosition({ driverPos.Long(), driverPos.Lat()});
        ptrDriver->SetPosRel(GSStatic::REL_ABSOLUTE);
        ptrDriver->SetHeading(m_initPos.Heading);
        ptrDriver->GetInitPos().Altitude = 1.0961 + m_initPos.Altitude;
        ptrDriver->GetInitPos().OnGround = 0;
        ptrDriver->PreSpawn();
        m_attached.emplace_back(ptrDriver);
    }

    return true;
}

void GSPushback::SetFinalPositionAndState()
{
    Freeze();
}
}
