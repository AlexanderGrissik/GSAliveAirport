// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSUtilityVehicle.h"
#include "General/GSCatalog.h"

namespace NS_GSAliveAirport
{

bool GSUtilityVehicle::PreSpawn()
{
    m_title = GSCatalog::GetInstance().GetUtilityVehicle();
    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    m_initPos.OnGround = 1;
    m_initPos.Altitude = 0;

    PrepareInitPos();

    return true;
}

} // namespace NS_GSAliveAirport
