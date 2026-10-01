// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSAircraftGroundSmall.h"
#include "GSPushback.h"
#include "GSTruckFacility.h"
#include "GSSmallCones.h"
#include "GSWingmans.h"

namespace NS_GSAliveAirport
{

void GSAircraftGroundSmall::BuildObjs()
{
    m_objs.emplace_back(new GSPushback(m_simConnect, m_aircraft, *this, GSPushback::PUSH_SMALL));
    m_objs.emplace_back(new GSTruckFacility(m_simConnect, m_aircraft, *this));
    m_objs.emplace_back(new GSSmallCones(m_simConnect, m_aircraft, *this));
    m_objs.emplace_back(new GSWingmans(m_simConnect, m_aircraft, *this, GSWingmans::SMALL));
}

}