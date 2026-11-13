// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSUtilityGround.h"
#include "GSUtilityVehicle.h"

namespace NS_GSAliveAirport
{

void GSUtilityGround::BuildObjs()
{
    size_t pathCount = m_airport->GetRoadNet().GetRoadPathCount();
    double pathMeters = m_airport->GetRoadNet().GetTotalVehiclePathLengthMeters();
    if (pathCount < 1 || pathMeters < 250.0) {
        return;
    }

    if (pathCount < 4 || pathMeters < 1000.0) {
        m_objs.emplace_back(new GSUtilityVehicle(m_simConnect, *m_airport, *this, GSUtilityVehicle::GENERAL));
    } else {
        size_t vehicleNum = static_cast<size_t>(pathMeters / 1000.0);
        while (vehicleNum-- > 0)
            m_objs.emplace_back(new GSUtilityVehicle(m_simConnect, *m_airport, *this,
                                GSUtilityVehicle::VehicleTypeFromIdx(vehicleNum)));
    }
}

void GSUtilityGround::Spawning()
{
    m_spawnState = SpawnState::SPAWNING;
    GSLogStream::Log("Utility Spawning for: ") << m_airport->GetICAO();
}

void GSUtilityGround::Spawned()
{
    m_spawnState = SpawnState::SPAWNED;
    GSLogStream::Log("Utility Spawned for: ") << m_airport->GetICAO();
}

void GSUtilityGround::Despawning()
{
    m_spawnState = SpawnState::DESPAWNING;
    GSLogStream::Log("Utility Despawning for: ") << m_airport->GetICAO();
}

void GSUtilityGround::Unspawned()
{
    m_spawnState = SpawnState::UNSPAWNED;
    GSLogStream::Log("Utility Despawned for: ") << m_airport->GetICAO();
}

} // namespace NS_GSAliveAirport
