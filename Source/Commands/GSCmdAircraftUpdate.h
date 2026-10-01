// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSCommand.h"
#include "../SimObjects/GSAircraft.h"

namespace NS_GSAliveAirport
{

class GSCmdAircraftUpdate : public GSCommand
{
public:

    GSCmdAircraftUpdate(int cmd, std::shared_ptr<GSAircraft>& ac): GSCommand(cmd), m_aircraft(ac) {}
    virtual ~GSCmdAircraftUpdate() {}

    const std::shared_ptr<GSAircraft>& GetAircraft() const { return m_aircraft; }

private:

    std::shared_ptr<GSAircraft> m_aircraft;
};
} // namespace NS_GSAliveAirport