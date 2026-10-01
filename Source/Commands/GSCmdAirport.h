// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "../SimObjects/GSSimObj.h"
#include "../General/GSAirport.h"
#include <memory>

namespace NS_GSAliveAirport
{

class GSCmdAirport : public GSCommand
{
public:

    GSCmdAirport(int cmd, std::shared_ptr<GSAirport>& airport) : GSCommand(cmd), m_airport(airport) {}
    virtual ~GSCmdAirport() {}

    std::shared_ptr<GSAirport>& Airport() { return m_airport; }

private:

    std::shared_ptr<GSAirport> m_airport;
};

} // namespace NS_GSAliveAirport
