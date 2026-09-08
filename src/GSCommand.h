#pragma once

#include "GSQuickQueue.h"
#include <functional>

class GSCommand;

using GSCmdQueue = GSQuickQueue<std::reference_wrapper<GSCommand>, 1024>;

class GSCommand
{
public:

    GSCommand(int cmd): m_commandID(cmd) {}
    GSCommand(int cmd, GSCmdQueue& queue): m_commandID(cmd), m_repQueue(&queue) {}

    int GetCmdID() const { return m_commandID; }

    void Finish() {
        if (m_repQueue)
            m_repQueue->Push(*this);
    }

    GSCommand& WaitPop() {
        if (m_repQueue)
            return m_repQueue->Pop();
        return *this;
    }

private:

    int m_commandID = 0;
    GSCmdQueue* m_repQueue = nullptr;
};



