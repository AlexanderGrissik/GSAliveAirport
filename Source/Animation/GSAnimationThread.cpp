// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSAnimationThread.h"
#include "../Commands/GSCmdAnimObj.h"
#include "../Commands/GSCmdAircraftUpdate.h"
#include "../General/GSLogStream.h"
#include "../General/GSGeography.h"
#include "GSAnimBFLO.h"
#include "GSAnimBFLOT.h"
#include "GSAnimSingle.h"

namespace NS_GSAliveAirport
{
using namespace std::chrono_literals;

void GSAnimationThread::Start()
{
    if (m_thread.joinable()) return;
    m_thread = std::jthread(&GSAnimationThread::AnimationLoop, this);
}

void GSAnimationThread::Stop()
{
    if (!m_thread.joinable()) return;
    m_thread.request_stop();
    m_thread.join();
}

void GSAnimationThread::AnimationLoop(std::stop_token stopToken, GSAnimationThread* self)
{
    self->RunLoopAnimation(stopToken);
}

void GSAnimationThread::RunLoopAnimation(std::stop_token stopToken)
{
    auto wakeTime = std::chrono::steady_clock::now();
    while (!stopToken.stop_requested()) {
        RunDispatch(stopToken);
        
        if (IsSimStarted())
            DoAnimation();

        if (m_animObjs.size())
            std::this_thread::sleep_for(30ms - (m_lastAnimTime - wakeTime));
        else
            std::this_thread::sleep_for(1s);
            
        wakeTime = std::chrono::steady_clock::now();
    }

    OnDisconnect();
    Disconnect();
}

void GSAnimationThread::OnConnect()
{
    GSAnimationObject::InitDatums(*this);
    GSAnimBFLO::InitDatums(*this);
    GSAnimBFLOT::InitDatums(*this);
    GSAnimSingle::InitDatums(*this);
}

void GSAnimationThread::DoAnimation()
{
    auto currTime = std::chrono::steady_clock::now();
    for (auto& anim : m_animObjs)
        if (GSGeography::DistanceDegPow(m_userAircraft.GetLongLat(), anim.second->GetCurrLoc()) < s_AnimDistDegPow)
            anim.second->Animate(*this, std::chrono::duration<float, std::milli>(currTime - m_lastAnimTime).count());

    m_lastAnimTime = std::chrono::steady_clock::now();
}

void GSAnimationThread::OnCommand(GSCommand& cmd)
{
    switch (cmd.GetCmdID()) {
    case GS_CMD_ANIM_OBJ_ADD: {
        auto& cmdAnim = static_cast<GSCmdAnimObj&>(cmd);
        m_animObjs.emplace(cmdAnim.ObjID(), cmdAnim.AnimObj().release());
        break;
    }
    case GS_CMD_ANIM_OBJ_REM: {
        auto& cmdAnim = static_cast<GSCmdAnimObj&>(cmd);
        m_animObjs.erase(cmdAnim.ObjID());
        cmdAnim.RetSimConn().PostCommand(
            new GSCmdAnimObj(GS_CMD_ANIM_OBJ_REM_DONE, *this, cmdAnim.ObjID(), cmdAnim.ReleaseReq()));
        break;
    }
    case GS_CMD_AIRCRAFT_USER:
        // Empty command is an alternative to aquire fence.
        break;
    default:
        GSLogStream::LogError("GSAnimationThread - Unexpected cmd: ") << cmd.GetCmdID();
        break;
    }
}

}