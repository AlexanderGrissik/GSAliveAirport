#include "GSAircraftGround.h"
#include <algorithm>
#include "../General/GSLogStream.h"

namespace NS_GSLiveAirportMSFS
{

void GSAircraftGround::Spawn()
{
    if (m_spawnState == SpawnState::SPAWNING || m_spawnState == SpawnState::SPAWNED) {
        GSLogStream::LogError("Double Spawn: ") << m_aircraft.GetObjID();
        return;
    }

    m_spawnState = SpawnState::SPAWNING;
    BuildObjs();

    std::erase_if(m_objs, [](const auto& item) {
        return !item->PreSpawn(); // remove items that do not match
    });

    if (m_objs.empty())
        m_spawnState = SpawnState::SPAWNED;

    for (auto& itr : m_objs)
        itr->Spawn();
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

    if (m_spawnedObjs == m_objs.size())
        m_spawnState = SpawnState::SPAWNED;
}

void GSAircraftGround::OnDespawned(bool ok, GSSimObj& obj)
{
    (void)obj;
    (void)ok;
    --m_spawnedObjs;
    if (!m_spawnedObjs) {
        m_objs.clear();
        m_spawnState = SpawnState::UNSPAWNED;
    }
}

}