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
#include "GSStaircase.h"

namespace NS_GSAliveAirport
{

void GSAircraftGroundXL::BuildObjs(bool full)
{
	m_objs.emplace_back(new GSLinerCones(m_simConnect, m_aircraft, *this));
    m_objs.emplace_back(new GSLavatoryTruck(m_simConnect, m_aircraft, *this));
	m_objs.emplace_back(new GSBuggageLoader(m_simConnect, m_aircraft, *this, true, m_aircraft.IsDeparting()));
	m_objs.emplace_back(new GSBuggageLoader(m_simConnect, m_aircraft, *this, false, m_aircraft.IsDeparting()));
	m_objs.emplace_back(new GSWingmans(m_simConnect, m_aircraft, *this, GSWingmans::AIRLINE));

	if (m_aircraft.IsDeparting()) {
		m_objs.emplace_back(new GSCateringCart(m_simConnect, m_aircraft, *this));
	} else {
		m_objs.emplace_back(new GSMarshaller(m_simConnect, m_aircraft, *this));
	}

	const auto* prk = m_aircraft.GetParking();
	if (!prk || !prk->m_hasJetway) {
		m_objs.emplace_back(new GSStaircase(m_simConnect, m_aircraft, *this, true, m_aircraft.IsDeparting()));
		m_objs.emplace_back(new GSStaircase(m_simConnect, m_aircraft, *this, false, m_aircraft.IsDeparting()));
	}

	if (full) {
		m_objs.emplace_back(new GSGroundPower(m_simConnect, m_aircraft, *this, GSGroundPower::GPU_DEFAULT));

		if (m_aircraft.IsDeparting()) {
			m_objs.emplace_back(new GSPushback(m_simConnect, m_aircraft, *this, GSPushback::PUSH_XL));
		}
	}
}

}