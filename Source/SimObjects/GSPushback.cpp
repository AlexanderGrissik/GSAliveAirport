// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSPushback.h"
#include "../General/GSGeography.h"
#include "../General/GSCatalog.h"
#include "GSStatic.h"

namespace NS_GSLiveAirportMSFS
{

bool GSPushback::OnCreated()
{
    FinalizeRoute();
    return false;
}

void GSPushback::OnArrived()
{
    Freeze();
    ContinueSpawn();
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

    PrepareRoute();

    return true;
}

}
