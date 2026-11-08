// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSCommand.h"
#include "General/GSAirport.h"
#include <memory>

namespace NS_GSAliveAirport
{

class GSCmdAirport : public GSCommand
{
public:
    GSCmdAirport(GSCommandID cmd, std::shared_ptr<GSAirport>& airport):
        GSCommand(cmd), m_airport(airport) {}
    ~GSCmdAirport() override = default;

    std::shared_ptr<GSAirport>& GetAirport() { return m_airport; }

private:
    std::shared_ptr<GSAirport> m_airport;
};

} // namespace NS_GSAliveAirport
