#pragma once

#include "../General/GSQuickQueue.h"
#include <functional>

namespace NS_GSLiveAirportMSFS
{

class GSCommand;

using CmdPtr = std::unique_ptr<GSCommand>;
using GSCmdQueue = GSQuickQueue<CmdPtr, 1024>;

class GSCommand
{
public:

    GSCommand(int cmd): m_commandID(cmd) {}
    GSCommand(int cmd, GSCmdQueue& queue): m_commandID(cmd), m_repQueue(&queue) {}
    virtual ~GSCommand() {}

    int GetCmdID() const { return m_commandID; }

    void Finish(CmdPtr& cmd) {
        if (m_repQueue)
            m_repQueue->Push(cmd);
    }

private:

    int m_commandID = 0;
    GSCmdQueue* m_repQueue = nullptr;
};
} // namespace NS_GSLiveAirportMSFS



