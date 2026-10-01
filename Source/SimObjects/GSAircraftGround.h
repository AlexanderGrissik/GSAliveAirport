// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSAircraft.h"
#include "../General/GSAirport.h"
#include "GSSimObj.h"
#include <cstddef>
#include <list>
#include <memory>

namespace NS_GSAliveAirport
{

class GSAircraftGround : public GSSimObjUpdate
{
public:

    enum SpawnState
    {
        UNSPAWNED = 0,
        SPAWNING,
        SPAWNED,
        DESPAWNING
    };

    GSAircraftGround(const GSAircraft& aircraft, GSSimConnect& simConn, std::shared_ptr<GSAirport>& airport);
    virtual ~GSAircraftGround() = default;

    virtual void BuildObjs() = 0;

    void OnSpawned(bool ok, GSSimObj& obj) override;
    void OnDespawned() override;
    
    void Spawn();
    void Despawn();
    void UpdateAircraft(const GSAircraft& aircraft) { m_aircraft.CopyDynInfo(aircraft); }
    SpawnState GetSpawnState() const { return m_spawnState; }
    const GSAircraft& Aircraft() const { return m_aircraft; }

protected:

    GSAircraft m_aircraft;
    std::shared_ptr<GSAirport> m_airport;
    GSSimConnect& m_simConnect;
    SpawnState m_spawnState = SpawnState::UNSPAWNED;
    std::list<std::unique_ptr<GSSimObj>> m_objs;
    size_t m_spawnedObjs = 0U;
};

}
