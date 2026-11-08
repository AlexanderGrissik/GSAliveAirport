// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSAircraftGround.h"
#include <algorithm>
#include "General/GSLogStream.h"
#include "General/GSRandom.h"
#include "UI/GSAliveUIAPI.h"
#include "GSCateringCart.h"
#include "GSGroundPower.h"
#include "GSLinerCones.h"
#include "GSSmallCones.h"
#include "GSPushback.h"
#include "GSLavatoryTruck.h"
#include "GSBuggageLoader.h"
#include "GSMarshaller.h"
#include "GSWingmans.h"
#include "GSStaircase.h"
#include "GSTruckFacility.h"

namespace NS_GSAliveAirport
{

void GSAircraftGround::BuildObjs()
{
    m_aircraft.SetParking(nullptr);
    SetClosestParking();

    // MSFS injects traffic with parking assigned. Skip MSFS already provided ground services.
    const bool full = !m_aircraft.HasParking();
    const auto category = m_aircraft.GetCategory();
    if (category == GSAircraft::AircraftSizeCategory::Small) {
        m_objs.emplace_back(new GSTruckFacility(m_simConnect, m_aircraft, *this));
        m_objs.emplace_back(new GSSmallCones(m_simConnect, m_aircraft, *this));
        m_objs.emplace_back(new GSWingmans(m_simConnect, m_aircraft, *this, GSWingmans::SMALL));
        m_objs.emplace_back(new GSPushback(m_simConnect, m_aircraft, *this, GSPushback::PUSH_SMALL));
        return;
    }

    m_objs.emplace_back(new GSLinerCones(m_simConnect, m_aircraft, *this));
    m_objs.emplace_back(new GSLavatoryTruck(m_simConnect, m_aircraft, *this));
    m_objs.emplace_back(new GSBuggageLoader(m_simConnect, m_aircraft, *this, true));
    m_objs.emplace_back(new GSBuggageLoader(m_simConnect, m_aircraft, *this, false));
    m_objs.emplace_back(new GSWingmans(m_simConnect, m_aircraft, *this, GSWingmans::AIRLINE));

    if (m_aircraft.IsDeparting())
        m_objs.emplace_back(new GSCateringCart(m_simConnect, m_aircraft, *this));
    else
        m_objs.emplace_back(new GSMarshaller(m_simConnect, m_aircraft, *this));

    const auto* prk = m_aircraft.GetParking();
    if (!prk || !prk->m_hasJetway) {
        m_objs.emplace_back(new GSStaircase(m_simConnect, m_aircraft, *this, true));
        m_objs.emplace_back(new GSStaircase(m_simConnect, m_aircraft, *this, false));
    }

    if (full) {
        auto gpuType = GSGroundPower::GPU_MEDIUM;
        auto pushType = GSPushback::PUSH_DEFAULT;
        if (category == GSAircraft::AircraftSizeCategory::Large) {
            gpuType = GSGroundPower::GPU_LARGE;
        } else if (category == GSAircraft::AircraftSizeCategory::ExtraLarge) {
            gpuType = GSGroundPower::GPU_DEFAULT;
            pushType = GSPushback::PUSH_XL;
        }
        m_objs.emplace_back(new GSGroundPower(m_simConnect, m_aircraft, *this, gpuType));
        if (m_aircraft.IsDeparting())
            m_objs.emplace_back(new GSPushback(m_simConnect, m_aircraft, *this, pushType));
    }
}

void GSAircraftGround::UpdateAircraft(const GSAircraft& aircraft)
{ 
    m_aircraft.CopyDynInfo(aircraft);
    if (m_spawnState == SpawnState::UNSPAWNED) {
        if (!m_aircraft.GetParking() && m_aircraft.GetGroundSpeedKnots() < 0.5 && !m_aircraft.IsTaxing()) {
            SetClosestParking(); // For UI. Aircrafts that currently do not receive services.
        }
    }

    m_ui.PostMessageUI(GSAliveUIAPI::UpdateAircraft(m_aircraft));
}

void GSAircraftGround::Spawning()
{
    m_spawnState = SpawnState::SPAWNING;
    GSLogStream::Log("Services Spawning for: ") << m_aircraft.GetObjID();
    m_ui.PostMessageUI(GSAliveUIAPI::UpdateAircraft(m_aircraft));
    m_ui.PostMessageUI(GSAliveUIAPI::UpdateAircraftServices(m_aircraft.GetObjID(), GSAliveUIAPI::AircraftService::Spawning)); 
}

void GSAircraftGround::Spawned()
{
    m_spawnState = SpawnState::SPAWNED;
    // A completed spawn attempt can have no objects (filtered out or all failed).
    const bool hasServices = m_spawnedObjs > 0;
    GSLogStream::Log(hasServices ? "Services InPlace for: " : "No Services Spawned for: ") << m_aircraft.GetObjID();
    const auto service = hasServices ? GSAliveUIAPI::AircraftService::Inplace : GSAliveUIAPI::AircraftService::None;
    m_ui.PostMessageUI(GSAliveUIAPI::UpdateAircraftServices(m_aircraft.GetObjID(), service));
}

void GSAircraftGround::Despawning()
{
    m_spawnState = SpawnState::DESPAWNING;
    GSLogStream::Log("Services Despawning for: ") << m_aircraft.GetObjID();
    m_ui.PostMessageUI(GSAliveUIAPI::UpdateAircraftServices(m_aircraft.GetObjID(), GSAliveUIAPI::AircraftService::Despawning));
}

void GSAircraftGround::Unspawned()
{
    m_spawnState = SpawnState::UNSPAWNED;
    GSLogStream::Log("Services Despawned for: ") << m_aircraft.GetObjID();
    m_ui.PostMessageUI(GSAliveUIAPI::UpdateAircraftServices(m_aircraft.GetObjID(), GSAliveUIAPI::AircraftService::None));
}

void GSAircraftGround::SetClosestParking()
{
    const auto prk = m_aircraft.GetAirport()->GetRoadNet().GetClosestNormalParking(m_aircraft.GetLongLat());
    if (prk.has_value()) {
        m_aircraft.SetParking(prk.value());
    }
}

}
