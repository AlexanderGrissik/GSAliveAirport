#include "GSStatic.h"
#include "../GSGeography.h"
#include "../GSCatalog.h"

namespace NS_GSLiveAirportMSFS
{

void GSStatic::OnCreated()
{
    SetFinalPositionAndState();
    OnObjSpawned(true);
}

bool GSStatic::PreSpawn()
{
    const auto& airData = m_aircraft.GetRawData();
    
    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    
    if (m_posRel == REL_AIRCRAFT) {
        m_initPos.Heading = GSGeography::NormDeg(airData.headingDegrees + 180.0);
        const auto spawnPos = GSGeography::RelativePosition(
            airData.headingDegrees, m_aircraft.GetLongLat(), m_initPos.Latitude, m_initPos.Longitude);
        m_initPos.Longitude = spawnPos.Long();
        m_initPos.Latitude = spawnPos.Lat();
        m_initPos.Altitude = airData.groundAltitudeFeet;
        m_initPos.OnGround = 1;
    }  

    return true;
}

void GSStatic::SetFinalPositionAndState()
{
    Freeze();

    if (m_animObj.get()) {
        m_animObj->ResetObjID(m_simObjectID);
        RegisterAnim(m_animObj.release());
    }
}
}
