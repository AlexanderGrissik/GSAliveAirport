// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSAircraft.h"
#include "General/GSAirport.h"
#include "UI/GSAliveUI.h"
#include "GSGroupGround.h"
#include <cstddef>
#include <list>
#include <memory>

namespace NS_GSAliveAirport
{

class GSAircraftGround : public GSGroupGround
{
public:

    GSAircraftGround(std::shared_ptr<GSAircraft>& aircraft, GSSimConnect& simConn, GSAliveUI& ui) :
        GSGroupGround(simConn, ui), m_aircraft(*aircraft) {}
    virtual ~GSAircraftGround() = default;

    void BuildObjs() override;
    void QuickArrive();

    void UpdateAircraft(const GSAircraft& aircraft);
    const GSAircraft& Aircraft() const { return m_aircraft; }

protected:

    void Spawning() override;
    void Spawned() override;
    void Despawning() override;
    void Unspawned() override;
    void SetClosestParking();

    GSAircraft m_aircraft;
};

}
