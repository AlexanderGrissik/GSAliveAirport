#pragma once

#include "GSAircraft.h"
#include "../GSSimConnect.h"

namespace NS_GSLiveAirportMSFS
{

class GSSimConnect;

class GSAircraftGround
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

    void Spawn();
    void Despawn();
    void UpdateAircraft(const GSAircraft& aircraft) { m_aircraft = aircraft; }
    SpawnState GetSpawnState() const { return m_spawnState; }
    const GSAircraft& Aircraft() const { return m_aircraft; }

private:

    GSAircraft m_aircraft;
    GSSimConnect& m_simConnect;
    SpawnState m_spawnState = SpawnState::UNSPAWNED;
};

}