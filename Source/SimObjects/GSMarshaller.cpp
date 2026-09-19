#include "GSMarshaller.h"
#include "../General/GSGeography.h"
#include "../General/GSCatalog.h"
#include "../Animation/GSAnimSingle.h"
#include "../SimObjects/GSAircraft.h"

namespace NS_GSLiveAirportMSFS
{

bool GSMarshaller::OnCreated()
{
    Freeze();

    if (m_hasAnimMarshall) {
        auto* anim = new GSAnimSingle(m_simObjectID);
        anim->AddFrameSet(GSDefinitions::GSDefID_AnimVelocBodyY, 0, 2179, 30.0);
        RegisterAnim(anim);
    }

    return true;
}

bool GSMarshaller::PreSpawn()
{
    const auto& airData = m_aircraft.GetRawData();
    auto& title = GSCatalog::GetInstance().GetMarshallerExt();
    if (title.empty()) {
        m_hasAnimMarshall = false;
        m_title = GSCatalog::GetInstance().GetMarshaller();
    } else {
        m_title = title;
    }
        
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

}
