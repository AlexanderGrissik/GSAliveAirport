// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSPaxBus.h"

namespace NS_GSAliveAirport
{

bool GSPaxBus::OnCreated()
{
    FinalizeRoute();
    return false;
}

void GSPaxBus::OnArrived()
{
    Freeze();
    m_isArrived = true;
    ContinueSpawn();
}

bool GSPaxBus::PreSpawn()
{
    m_title = "Bus Apron 02";

    const auto& airData = m_aircraft.GetRawData();
    m_initPos.Heading = m_targetHeading;
    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    m_initPos.OnGround = 1;
    m_initPos.Altitude = airData.groundAltitudeFeet;

    m_initPos.Longitude = m_targetPos.Long();
    m_initPos.Latitude = m_targetPos.Lat();

    PrepareRoute();

    return true;
}

} // namespace NS_GSAliveAirport
