#include "GSAircraftGround.h"

namespace NS_GSLiveAirportMSFS
{

void GSAircraftGround::Spawn()
{
    m_spawnState = SpawnState::SPAWNED;
}

void GSAircraftGround::Despawn()
{
    m_spawnState = SpawnState::UNSPAWNED;
}

}