// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSMarshaller.h"
#include "../General/GSGeography.h"
#include "../General/GSCatalog.h"
#include "../Animation/GSAnimSingle.h"
#include "../SimObjects/GSAircraft.h"

namespace NS_GSAliveAirport
{

bool GSMarshaller::OnCreated()
{
    Freeze();

    if (m_hasAnimMarshall) {
        GSCoord initPosLoc = { m_initPos.Longitude, m_initPos.Latitude };
        auto* anim = new GSAnimSingle(m_simObjectID, initPosLoc, GSDefinitions::GSDefID_AnimVelocBodyY);
        anim->AddFrameSet({ 0, 2179, 30.0 });
        RegisterAnim(anim);
    }

    return true;
}

bool GSMarshaller::PreSpawn()
{
    auto& title = GSCatalog::GetInstance().GetMarshallerExt();
    if (title.empty()) {
        m_hasAnimMarshall = false;
        m_title = GSCatalog::GetInstance().GetMarshaller();
    } else {
        m_title = title;
    }
        
    m_initPos.Heading = GSGeography::NormDeg(m_aircraft.GetHeading() + 180);
    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    m_initPos.OnGround = 1;
    m_initPos.Altitude = m_aircraft.GetGroundAltitudeFeet();
    
    auto halfWing = m_aircraft.GetWingSpanMeters() / 2.0;
    const auto locPos = GSGeography::RelativePosition(m_aircraft.GetHeading(), m_aircraft.GetLongLat(), halfWing + 7, 0);
    m_initPos.Longitude = locPos.Long();
    m_initPos.Latitude = locPos.Lat();

    return true;
}

}
