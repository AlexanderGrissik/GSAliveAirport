// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSUtilityGround.h"

namespace NS_GSAliveAirport
{

void GSUtilityGround::BuildObjs()
{
}

void GSUtilityGround::Spawning()
{
    m_spawnState = SpawnState::SPAWNING;
}

void GSUtilityGround::Spawned()
{
    m_spawnState = SpawnState::SPAWNED;
}

void GSUtilityGround::Despawning()
{
    m_spawnState = SpawnState::DESPAWNING;
}

void GSUtilityGround::Unspawned()
{
    m_spawnState = SpawnState::UNSPAWNED;
}

} // namespace NS_GSAliveAirport
