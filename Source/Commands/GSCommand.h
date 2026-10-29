// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "../General/GSDefinitions.h"
#include "../General/GSQuickQueue.h"
#include <memory>
#include <optional>

namespace NS_GSAliveAirport
{

class GSCommand
{
public:

    GSCommand(GSCommandID cmd): m_commandID(cmd) {}
    GSCommand(GSCommandID cmd, GSTimePoint execTime): m_commandID(cmd), m_execTime(execTime) {}
    virtual ~GSCommand() {}

    GSCommandID GetCmdID() const { return m_commandID; }
    const std::optional<GSTimePoint>& GetExecTime() const { return m_execTime; }

private:

    GSCommandID m_commandID;
    std::optional<GSTimePoint> m_execTime;
};

using GSCommandU = std::unique_ptr<GSCommand>;
using GSCommandUQueue = GSQuickQueue<GSCommandU, 1024>;

} // namespace NS_GSAliveAirport



