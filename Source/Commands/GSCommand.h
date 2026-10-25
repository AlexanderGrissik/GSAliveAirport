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

class GSCommand;

using CmdPtr = std::unique_ptr<GSCommand>;
using GSCmdQueue = GSQuickQueue<CmdPtr, 1024>;

class GSCommand
{
public:

    GSCommand(int cmd): m_commandID(cmd) {}
    GSCommand(int cmd, GSTimePoint execTime): m_commandID(cmd), m_execTime(execTime) {}
    GSCommand(int cmd, GSCmdQueue& queue): m_commandID(cmd), m_repQueue(&queue) {}
    virtual ~GSCommand() {}

    int GetCmdID() const { return m_commandID; }
    const std::optional<GSTimePoint>& GetExecTime() const { return m_execTime; }

    void Finish(CmdPtr& cmd) {
        if (m_repQueue)
            m_repQueue->Push(cmd);
    }

private:

    int m_commandID = 0;
    std::optional<GSTimePoint> m_execTime;
    GSCmdQueue* m_repQueue = nullptr;
};
} // namespace NS_GSAliveAirport



