// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "../SimObjects/GSSimObj.h"
#include "../General/GSSimConnect.h"

namespace NS_GSAliveAirport
{

class GSCmdSimObj : public GSCommand
{
public:

    GSCmdSimObj(int cmd, GSSimObj& simObj) : GSCommand(cmd), m_simObj(simObj) {}
    virtual ~GSCmdSimObj() {}

    GSSimObj& SimObj() { return m_simObj; }
        
private:

    GSSimObj& m_simObj;
};

} // namespace NS_GSAliveAirport
