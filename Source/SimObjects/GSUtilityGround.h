// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSGroupGround.h"
#include "General/GSAirport.h"
#include <memory>
#include <utility>

namespace NS_GSAliveAirport
{

class GSUtilityGround : public GSGroupGround
{
public:
    GSUtilityGround(std::shared_ptr<GSAirport> airport, GSSimConnect& simConn, GSAliveUI& ui):
        GSGroupGround(simConn, ui), m_airport(std::move(airport)) {}
    ~GSUtilityGround() override = default;

    void BuildObjs() override;
    const GSAirport* GetAirport() const { return m_airport.get(); }

protected:
    void Spawning() override;
    void Spawned() override;
    void Despawning() override;
    void Unspawned() override;

    std::shared_ptr<GSAirport> m_airport;
};

} // namespace NS_GSAliveAirport
