// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSWingmans.h"
#include "../General/GSGeography.h"
#include "../General/GSCatalog.h"
#include "GSStatic.h"
#include "../Animation/GSAnimSingle.h"

namespace NS_GSAliveAirport
{

bool GSWingmans::OnCreated()
{
    Freeze();

    auto* anim = new GSAnimSingle(m_simObjectID);
    anim->AddFrameSet(GSDefinitions::GSDefID_AnimVelocBodyY, 230, 267, 30.0);
    anim->SetAIWaypoints(&m_aiWaypoints);
    RegisterAnim(anim);
    
    return true;
}

bool GSWingmans::PreSpawn()
{
    const auto& airData = m_aircraft.GetRawData();

    m_title = GSCatalog::GetInstance().GetWingwalker();
    if (m_title.empty())
        return false;

    m_initPos.Heading = airData.headingDegrees;
    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    m_initPos.OnGround = 1;
    m_initPos.Altitude = airData.groundAltitudeFeet;

    double YShift = (m_countTP == GSWingmans::AIRLINE ? 8 : 5);
    auto halfWing = airData.wingSpanMeters / 2.0;
    const auto locPos = GSGeography::RelativePosition(
        airData.headingDegrees, m_aircraft.GetLongLat(), -halfWing - YShift - 6, 0);
    m_initPos.Longitude = locPos.Long();
    m_initPos.Latitude = locPos.Lat();
    
    const auto wp1 = GSGeography::RelativePosition(
        airData.headingDegrees, m_aircraft.GetLongLat(), -halfWing - YShift, -halfWing / 2);
    const auto wp2 = GSGeography::RelativePosition(
        airData.headingDegrees, m_aircraft.GetLongLat(), -halfWing - YShift, halfWing / 2);
    BuildWalkPath(wp1, wp2, *this);
    
    if (m_countTP == GSWingmans::AIRLINE) {
        auto ptrWalker2 = new GSStatic(m_simHandle, m_aircraft, *this);
        ptrWalker2->SetTitle(m_title);
        ptrWalker2->SetPosition({ halfWing - 6, -halfWing / 2 });
        ptrWalker2->PreSpawn();

        auto* animWorker2 = new GSAnimSingle(0);
        animWorker2->AddFrameSet(GSDefinitions::GSDefID_AnimVelocBodyY, 230, 267, 30.0);
        ptrWalker2->SetAnimObj(animWorker2);

        const auto wp21 = GSGeography::RelativePosition(
            airData.headingDegrees, m_aircraft.GetLongLat(), 0, halfWing);
        const auto wp22 = GSGeography::RelativePosition(
            airData.headingDegrees, m_aircraft.GetLongLat(), -halfWing, halfWing);
        BuildWalkPath(wp21, wp22, *ptrWalker2);

        m_attached.emplace_back(ptrWalker2);

        auto ptrWalker3 = new GSStatic(m_simHandle, m_aircraft, *this);
        ptrWalker3->SetTitle(m_title);
        ptrWalker3->SetPosition({ -halfWing + 6, -halfWing / 2 });
        ptrWalker3->PreSpawn();

        auto* animWorker3 = new GSAnimSingle(0);
        animWorker3->AddFrameSet(GSDefinitions::GSDefID_AnimVelocBodyY, 230, 267, 30.0);
        ptrWalker3->SetAnimObj(animWorker3);

        const auto wp31 = GSGeography::RelativePosition(
            airData.headingDegrees, m_aircraft.GetLongLat(), 0, -halfWing);
        const auto wp32 = GSGeography::RelativePosition(
            airData.headingDegrees, m_aircraft.GetLongLat(), -halfWing, -halfWing);
        BuildWalkPath(wp31, wp32, *ptrWalker3);

        m_attached.emplace_back(ptrWalker3);
    }

    return true;
}

void GSWingmans::BuildWalkPath(const GSCoord& wp1, const GSCoord& wp2, GSSimObj& simObj)
{
    GSCoord center = { simObj.GetInitPos().Longitude, simObj.GetInitPos().Latitude };
    static unsigned flags = SIMCONNECT_WAYPOINT_ON_GROUND | SIMCONNECT_WAYPOINT_SPEED_REQUESTED;
    float alt = static_cast<float>(simObj.GetInitPos().Altitude);
    float speedKTs = 2.2f;
    simObj.AddWaypoint(center, alt, speedKTs, 0.0, flags);
    simObj.AddWaypoint(wp1, alt, speedKTs, 0.0, flags);
    simObj.AddWaypoint(wp2, alt, speedKTs, 0.0, flags);
    simObj.AddWaypoint(center, alt, speedKTs, 0.0, flags);
    simObj.AddWaypoint(wp2, alt, speedKTs, 0.0, flags);
    simObj.AddWaypoint(wp1, alt, speedKTs, 0.0, flags);
    simObj.AddWaypoint(center, alt, speedKTs, 0.0, flags | SIMCONNECT_WAYPOINT_WRAP_TO_FIRST);
}

}
