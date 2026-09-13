#pragma once

#include "GSAircraft.h"
#include "../GSSimConnect.h"
#include "GSSimObj.h"
#include <list>

namespace NS_GSLiveAirportMSFS
{

class GSSimConnect;

class GSAircraftGround : public GSSimObj::IObjUpdate
{
public:

    enum SpawnState
    {
        UNSPAWNED = 0,
        SPAWNING,
        SPAWNED,
        DESPAWNING
    };

    GSAircraftGround(const GSAircraft& aircraft, GSSimConnect& simConn): m_aircraft(aircraft), m_simConnect(simConn) {}
    virtual ~GSAircraftGround() = default;

    virtual void BuildObjs() = 0;

    void OnSpawned(bool ok, GSSimObj& obj) override;
    void OnDespawned(bool ok, GSSimObj& obj) override;
    
    void Spawn();
    void Despawn();
    void UpdateAircraft(const GSAircraft& aircraft) { m_aircraft = aircraft; }
    SpawnState GetSpawnState() const { return m_spawnState; }
    const GSAircraft& Aircraft() const { return m_aircraft; }

protected:

    GSAircraft m_aircraft;
    GSSimConnect& m_simConnect;
    SpawnState m_spawnState = SpawnState::UNSPAWNED;
    std::list<std::unique_ptr<GSSimObj>> m_objs;
    size_t m_spawnedObjs = 0U;
};

}
