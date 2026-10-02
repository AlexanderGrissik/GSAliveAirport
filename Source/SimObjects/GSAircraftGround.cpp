// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSAircraftGround.h"
#include <algorithm>
#include "../General/GSLogStream.h"
#include "../General/GSRandom.h"

namespace NS_GSAliveAirport
{

GSAircraftGround::GSAircraftGround(const GSAircraft& aircraft, GSSimConnect& simConn, std::shared_ptr<GSAirport>& airport):
    m_aircraft(aircraft), m_simConnect(simConn), m_airport(airport)
{
    if (m_airport) {
        m_aircraft.SetAirport(m_airport.get());
        const auto prk = m_airport->GetRoadNet().GetClosestNormalParking(aircraft.GetLongLat());
        if (prk.has_value()) {
            m_aircraft.SetParking(prk.value());
        }
    }
}

void GSAircraftGround::Spawn()
{
    if (m_spawnState == SpawnState::SPAWNING || m_spawnState == SpawnState::SPAWNED) {
        GSLogStream::LogError("Double Spawn: ") << m_aircraft.GetObjID();
        return;
    }

    m_spawnState = SpawnState::SPAWNING;

    if (!m_aircraft.IsSleep()) {
        BuildObjs(true);
    } else if (GSRandom::RandSizeT(0, 1) == 0) {
        BuildObjs(false);
    }

    std::erase_if(m_objs, [](const auto& item) {
        return !item->PreSpawn(); // remove items that do not match
    });

    if (m_objs.empty())
        m_spawnState = SpawnState::SPAWNED;

    for (auto& itr : m_objs)
        itr->Spawn();

    GSLogStream::Log("Services Spawning for: ") << m_aircraft.GetObjID();
}

void GSAircraftGround::Despawn()
{
    if (m_spawnState == SpawnState::DESPAWNING || m_spawnState == SpawnState::UNSPAWNED) {
        GSLogStream::LogError("Double Despawn: ") << m_aircraft.GetObjID();
        return;
    }

    if (m_objs.size() > 0) {
        m_spawnState = SpawnState::DESPAWNING;
        for (auto& itr : m_objs)
            itr->Despawn();
        GSLogStream::Log("Services Despawning for: ") << m_aircraft.GetObjID();
    } else {
        m_spawnState = SpawnState::UNSPAWNED;
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
        GSLogStream::Log("Services InPlace for: ") << m_aircraft.GetObjID();
        m_spawnState = SpawnState::SPAWNED;
    }
}

void GSAircraftGround::OnDespawned()
{
    --m_spawnedObjs;
    if (!m_spawnedObjs) {
        m_objs.clear();
        GSLogStream::Log("Services Despawned for: ") << m_aircraft.GetObjID();
        m_spawnState = SpawnState::UNSPAWNED;
    }
}

}