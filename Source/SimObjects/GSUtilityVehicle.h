// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimUtilObj.h"

namespace NS_GSAliveAirport
{

class GSUtilityVehicle : public GSSimUtilObj
{
public:
    enum UtilityVehicleType {
        WATER = 0,
        FUEL,
        TRUCKUTIL,
        PICKUP,
        TUCKFACILITY,
        OPERTRUCK,
        CARUTIL,
        SUV,
        GENERAL
    };

    GSUtilityVehicle(GSSimConnect& simHandle, const GSAirport& airport, GSSimObjUpdate& parent, UtilityVehicleType vehicleType):
        GSSimUtilObj(simHandle, airport, parent), m_vehicleType(vehicleType) {}
    ~GSUtilityVehicle() override = default;

    static UtilityVehicleType VehicleTypeFromIdx(size_t idx);

    bool PreSpawn() override;

private:
    UtilityVehicleType m_vehicleType;
};

} // namespace NS_GSAliveAirport
