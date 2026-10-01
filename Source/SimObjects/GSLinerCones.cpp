// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSLinerCones.h"
#include "../General/GSGeography.h"
#include "../General/GSCatalog.h"
#include "GSStatic.h"
#include "../SimObjects/GSAircraft.h"

namespace NS_GSAliveAirport
{

bool GSLinerCones::OnCreated()
{
    Freeze();
    return true;
}

bool GSLinerCones::PreSpawn()
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
        airData.headingDegrees, m_aircraft.GetLongLat(), -halfWing - 6, -halfWing / 2);
    m_initPos.Longitude = conePos.Long();
    m_initPos.Latitude = conePos.Lat();
    
    auto ptrCone2 = new GSStatic(m_simHandle, m_aircraft, *this);
    ptrCone2->SetTitle("Cone_Medium");
    ptrCone2->SetPosition({ -halfWing / 4, -halfWing - 6 });
    ptrCone2->PreSpawn();
    m_attached.emplace_back(ptrCone2);

    auto ptrCone3 = new GSStatic(m_simHandle, m_aircraft, *this);
    ptrCone3->SetTitle("Cone_Medium");
    ptrCone3->SetPosition({ halfWing / 2, -halfWing - 6 });
    ptrCone3->PreSpawn();
    m_attached.emplace_back(ptrCone3);

    auto ptrCone4 = new GSStatic(m_simHandle, m_aircraft, *this);
    ptrCone4->SetTitle("Cone_Medium");
    ptrCone4->SetPosition({ halfWing / 4, -halfWing - 6 });
    ptrCone4->PreSpawn();
    m_attached.emplace_back(ptrCone4);

    return true;
}

}
