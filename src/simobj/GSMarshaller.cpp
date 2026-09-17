#include "GSMarshaller.h"
#include "../GSGeography.h"
#include "../GSCatalog.h"

namespace NS_GSLiveAirportMSFS
{

void GSMarshaller::OnCreated()
{
    SetFinalPositionAndState();
    OnObjSpawned(true);
}

bool GSMarshaller::PreSpawn()
{
    const auto& airData = m_aircraft.GetRawData();
    const auto& title = GSCatalog::GetInstance().GetMarshallerExt();
    if (title.empty())
        GSCatalog::GetInstance().GetMarshaller();

    m_title = title;
    m_initPos.Heading = GSGeography::NormDeg(airData.headingDegrees + 180);
    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    m_initPos.OnGround = 1;
    m_initPos.Altitude = airData.groundAltitudeFeet;
    
    auto halfWing = airData.wingSpanMeters / 2.0;
    const auto locPos = GSGeography::RelativePosition(airData.headingDegrees, m_aircraft.GetLongLat(), halfWing + 7, 0);
    m_initPos.Longitude = locPos.Long();
    m_initPos.Latitude = locPos.Lat();

    return true;
}

void GSMarshaller::SetFinalPositionAndState()
{
    Freeze();
}
}
