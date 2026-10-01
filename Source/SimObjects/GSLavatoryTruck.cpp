// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSLavatoryTruck.h"
#include "../General/GSGeography.h"
#include "../General/GSCatalog.h"
#include "../Animation/GSAnimBFLOT.h"
#include "../SimObjects/GSAircraft.h"

namespace NS_GSLiveAirportMSFS
{

bool GSLavatoryTruck::OnCreated()
{
    FinalizeRoute();
    return false;
}

void GSLavatoryTruck::OnArrived()
{
    if (m_hasAnimLavaratory) {
        auto* anim = new GSAnimBFLOT(m_simObjectID);
        anim->AddFrameSet(50, 51, 30.0, 0, 1755, 30, 0, 1942, 30);
        RegisterAnim(anim);
    }

    Freeze();
    ContinueSpawn();
}

bool GSLavatoryTruck::PreSpawn()
{
    const auto& airData = m_aircraft.GetRawData();
    auto rearLeftDoor = m_aircraft.GetRearLeftDoor();
    auto rearRightDoor = m_aircraft.GetRearRightDoor();
    const auto* refDoor = (rearLeftDoor.has_value() ? &rearLeftDoor->first.get() : nullptr);

    double YMeters = 0.0, XMeters = 0.0;
    if (!refDoor) {
        refDoor = (rearRightDoor.has_value() ? &rearRightDoor->first.get() : nullptr);
        if (!refDoor) {
            return false;
        }

        XMeters -= 3.0;
    }

    YMeters += refDoor->posZMeter - 3;
    XMeters += refDoor->posXMeter;

    if (!GSCatalog::GetInstance().GetLavatoryExt().empty()) {
        m_title = GSCatalog::GetInstance().GetLavatoryExt();
    } else {
        m_hasAnimLavaratory = false;
        m_title = "Microsoft_Truck_Lavatory";
    }
        
    m_initPos.Heading = GSGeography::NormDeg(airData.headingDegrees + 180.0);
    m_initPos.Heading += 5.0;

    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    m_initPos.OnGround = 1;
    m_initPos.Altitude = airData.groundAltitudeFeet;
    
    const auto doorPos = GSGeography::RelativePosition(
        airData.headingDegrees, m_aircraft.GetLongLat(), YMeters, XMeters);
    m_initPos.Longitude = doorPos.Long();
    m_initPos.Latitude = doorPos.Lat();

    PrepareRoute();

    return true;
}

}
