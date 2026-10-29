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

    GSCmdAircraftUpdate(GSCommandID cmd, DWORD aircraftID): GSCommand(cmd), m_aircraftID(aircraftID) {}
    virtual ~GSCmdAircraftUpdate() {}

    DWORD GetAircraftID() const { return m_aircraftID; }

private:

    DWORD m_aircraftID;
};
} // namespace NS_GSAliveAirport