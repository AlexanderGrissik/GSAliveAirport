// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSGroupGround.h"
#include "General/GSLogStream.h"

namespace NS_GSAliveAirport
{

void GSGroupGround::Spawn()
{
    if (m_spawnState == SpawnState::SPAWNING || m_spawnState == SpawnState::SPAWNED) {
        GSLogStream::LogError("GSGroupGround - Double Spawn");
        return;
    }

    BuildObjs();
    Spawning();

    std::erase_if(m_objs, [](const auto& item) {
        return !item->PreSpawn();
    });

    if (m_objs.empty()) {
        Spawned();
    }

    for (auto& obj : m_objs)
        obj->Spawn();
}

void GSGroupGround::Despawn()
{
    if (m_spawnState == SpawnState::DESPAWNING || m_spawnState == SpawnState::UNSPAWNED) {
        GSLogStream::LogError("GSGroupGround - Double Despawn");
        return;
    }

    if (!m_objs.empty()) {
        Despawning();
        for (auto& obj : m_objs)
            obj->Despawn();
    } else {
        Unspawned();
    }
}

void GSGroupGround::OnSpawned(bool ok, GSSimObj& obj)
{
    if (ok) {
        ++m_spawnedObjs;
    } else {
        std::erase_if(m_objs, [&obj](const auto& item) {
            return item.get() == &obj;
        });
    }

    if (m_spawnedObjs == m_objs.size()) {
        Spawned();
    }
}

void GSGroupGround::OnDespawned()
{
    --m_spawnedObjs;
    if (!m_spawnedObjs) {
        m_objs.clear();
        Unspawned();
    }
}

} // namespace NS_GSAliveAirport
