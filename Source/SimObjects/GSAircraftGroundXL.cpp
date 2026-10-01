// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSAircraftGroundXL.h"
#include "GSCateringCart.h"
#include "GSGroundPower.h"
#include "GSLinerCones.h"
#include "GSPushback.h"
#include "GSLavatoryTruck.h"
#include "GSBuggageLoader.h"
#include "GSMarshaller.h"
#include "GSWingmans.h"

namespace NS_GSAliveAirport
{

void GSAircraftGroundXL::BuildObjs()
{
	m_objs.emplace_back(new GSCateringCart(m_simConnect, m_aircraft, *this));
	m_objs.emplace_back(new GSLinerCones(m_simConnect, m_aircraft, *this));
    m_objs.emplace_back(new GSGroundPower(m_simConnect, m_aircraft, *this, GSGroundPower::GPU_DEFAULT));
	m_objs.emplace_back(new GSPushback(m_simConnect, m_aircraft, *this, GSPushback::PUSH_XL));
    m_objs.emplace_back(new GSLavatoryTruck(m_simConnect, m_aircraft, *this));
	m_objs.emplace_back(new GSBuggageLoader(m_simConnect, m_aircraft, *this, true));
	m_objs.emplace_back(new GSBuggageLoader(m_simConnect, m_aircraft, *this, false));
	m_objs.emplace_back(new GSMarshaller(m_simConnect, m_aircraft, *this));
	m_objs.emplace_back(new GSWingmans(m_simConnect, m_aircraft, *this, GSWingmans::AIRLINE));
}

}