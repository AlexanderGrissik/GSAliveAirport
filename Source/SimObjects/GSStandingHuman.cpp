// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSStandingHuman.h"
#include "../General/GSGeography.h"
#include "../General/GSCatalog.h"
#include "../SimObjects/GSAircraft.h"

namespace NS_GSLiveAirportMSFS
{

bool GSStandingHuman::OnCreated()
{
    Freeze();
    return true;
}

bool GSStandingHuman::PreSpawn()
{
    const auto& airData = m_aircraft.GetRawData();

    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    m_initPos.OnGround = 1;
    m_initPos.Altitude = airData.groundAltitudeFeet;

    const auto spawnPos = GSGeography::RelativePosition(
        airData.headingDegrees, m_aircraft.GetLongLat(), m_loc.Lat(), m_loc.Long());
    m_initPos.Longitude = spawnPos.Long();
    m_initPos.Latitude = spawnPos.Lat();
    m_initPos.Heading = GSGeography::Azz({ m_initPos.Longitude,m_initPos.Latitude }, m_aircraft.GetLongLat());
    auto& catalog = GSCatalog::GetInstance();

    if (m_humanType == PILOT) {
        const auto& title = catalog.GetPilotExt();
        m_title = (title.empty() ? catalog.GetTarmacHuman() : title);
    } else if (m_humanType == PASSENGER) {
        const auto& title = catalog.GetPassengerExt();
        m_title = (title.empty() ? catalog.GetTarmacHuman() : title);
    } 

    return true;
}

}
