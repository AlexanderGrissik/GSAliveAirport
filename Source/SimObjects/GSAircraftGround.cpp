// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSAircraftGround.h"
#include <algorithm>
#include "../General/GSLogStream.h"
#include "../General/GSRandom.h"
#include "../UI/GSAliveUIAPI.h"

namespace NS_GSAliveAirport
{

void GSAircraftGround::UpdateAircraft()
{ 
    if (m_spawnState == SpawnState::UNSPAWNED) {
        if (!m_aircraft->GetParking() && m_aircraft->GetGroundSpeedKnots() < 0.5 && !m_aircraft->IsTaxing()) {
            SetClosestParking(); // For UI. Aircrafts that currently do not receive services.
        }
    }

    m_ui.PostMessageUI(GSAliveUIAPI::UpdateAircraft(*m_aircraft));
}

void GSAircraftGround::Spawning()
{
    m_spawnState = SpawnState::SPAWNING;
    GSLogStream::Log("Services Spawning for: ") << m_aircraft->GetObjID();
    m_ui.PostMessageUI(GSAliveUIAPI::UpdateAircraft(*m_aircraft));
    m_ui.PostMessageUI(GSAliveUIAPI::UpdateAircraftServices(m_aircraft->GetObjID(), GSAliveUIAPI::AircraftService::Spawning)); 
}

void GSAircraftGround::Spawned()
{
    m_spawnState = SpawnState::SPAWNED;
    // A completed spawn attempt can have no objects (filtered out or all failed).
    const bool hasServices = m_spawnedObjs > 0;
    GSLogStream::Log(hasServices ? "Services InPlace for: " : "No Services Spawned for: ") << m_aircraft->GetObjID();
    const auto service = hasServices ? GSAliveUIAPI::AircraftService::Inplace : GSAliveUIAPI::AircraftService::None;
    m_ui.PostMessageUI(GSAliveUIAPI::UpdateAircraftServices(m_aircraft->GetObjID(), service));
}

void GSAircraftGround::Despawning()
{
    m_spawnState = SpawnState::DESPAWNING;
    GSLogStream::Log("Services Despawning for: ") << m_aircraft->GetObjID();
}

void GSAircraftGround::Unspawned()
{
    m_spawnState = SpawnState::UNSPAWNED;
    GSLogStream::Log("Services Despawned for: ") << m_aircraft->GetObjID();
    m_ui.PostMessageUI(GSAliveUIAPI::UpdateAircraftServices(m_aircraft->GetObjID(), GSAliveUIAPI::AircraftService::None));
}

void GSAircraftGround::SetClosestParking()
{
    const auto prk = m_aircraft->GetAirport()->GetRoadNet().GetClosestNormalParking(m_aircraft->GetLongLat());
    if (prk.has_value()) {
        m_aircraft->SetParking(prk.value());
    }
}

void GSAircraftGround::Spawn()
{
    if (m_spawnState == SpawnState::SPAWNING || m_spawnState == SpawnState::SPAWNED) {
        GSLogStream::LogError("Double Spawn: ") << m_aircraft->GetObjID();
        return;
    }

    m_aircraft->SetParking(nullptr);
    SetClosestParking();

    Spawning(); 

    // MSFS injects traffic with parking assigned. Skip MSFS already provided ground services.
    BuildObjs(!m_aircraft->HasParking());

    std::erase_if(m_objs, [](const auto& item) {
        return !item->PreSpawn(); // remove items that do not match
    });

    if (m_objs.empty()) {
        Spawned();
    }

    for (auto& itr : m_objs)
        itr->Spawn();
}

void GSAircraftGround::Despawn()
{
    if (m_spawnState == SpawnState::DESPAWNING || m_spawnState == SpawnState::UNSPAWNED) {
        GSLogStream::LogError("Double Despawn: ") << m_aircraft->GetObjID();
        return;
    }

    if (m_objs.size() > 0) {
        Despawning();
        for (auto& itr : m_objs)
            itr->Despawn();
    } else {
        Unspawned(); 
    }
}

void GSAircraftGround::OnSpawned(bool ok, GSSimObj& obj)
{
    if (ok) {
        ++m_spawnedObjs;
    } else {
        std::erase_if(m_objs, [&obj](const auto& item) {
            return (item.get() == &obj); // remove items that do not match
        });
    }

    if (m_spawnedObjs == m_objs.size()) {
        Spawned(); 
    }
}

void GSAircraftGround::OnDespawned()
{
    --m_spawnedObjs;
    if (!m_spawnedObjs) {
        m_objs.clear();
        Unspawned(); 
    }
}

}
