// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSUtilityVehicle.h"
#include "General/GSCatalog.h"

namespace NS_GSAliveAirport
{

GSUtilityVehicle::UtilityVehicleType GSUtilityVehicle::VehicleTypeFromIdx(size_t idx)
{
    if (idx > static_cast<size_t>(GENERAL))
        return GENERAL;

    return static_cast<UtilityVehicleType>(idx);
}

bool GSUtilityVehicle::PreSpawn()
{
    switch (m_vehicleType) {
    case WATER:
        m_title = GSCatalog::GetInstance().GetWaterTruck();
        break;
    case FUEL:
        m_title = GSCatalog::GetInstance().GetFuelTruck();
        break;
    case TRUCKUTIL:
        m_title = GSCatalog::GetInstance().GetUtilityTruck();
        break;
    case PICKUP:
        m_title = GSCatalog::GetInstance().GetPickup();
        break;
    case TUCKFACILITY:
        m_title = GSCatalog::GetInstance().GetUtilityTruckFacility();
        break;
    case OPERTRUCK:
        m_title = GSCatalog::GetInstance().GetOperationTruck();
        break;
    case CARUTIL:
        m_title = "ASO_CarUtility01";
        break;
    case SUV:
        m_title = "SUV_NorthAm";
        break;
    default:
        m_title = GSCatalog::GetInstance().GetUtilityVehicle();
        break;
    }

    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    m_initPos.OnGround = 1;
    m_initPos.Altitude = 0;

    PrepareInitPos();

    return true;
}

} // namespace NS_GSAliveAirport
