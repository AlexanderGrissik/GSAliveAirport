#include "GSAnimationThread.h"
#include "../Commands/GSCmdAnimObj.h"
#include "../General/GSLogStream.h"

namespace NS_GSLiveAirportMSFS
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
}

void GSAnimationThread::DoAnimation()
{
    auto currTime = std::chrono::steady_clock::now();
    for (auto& anim : m_animObjs)
        anim.second->Animate(*this, std::chrono::duration<float, std::milli>(currTime - m_lastAnimTime).count());

    m_lastAnimTime = std::chrono::steady_clock::now();
}

void GSAnimationThread::OnCommand(GSCommand& cmd)
{
    switch (cmd.GetCmdID()) {
    case GSDefinitions::CMD_ANIM_OBJ_ADD: {
        auto& cmdAnim = static_cast<GSCmdAnimObj&>(cmd);
        m_animObjs.emplace(cmdAnim.ObjID(), cmdAnim.AnimObj().release());
        break;
    }
    case GSDefinitions::CMD_ANIM_OBJ_REM: {
        auto& cmdAnim = static_cast<GSCmdAnimObj&>(cmd);
        m_animObjs.erase(cmdAnim.ObjID());
        CmdPtr newCmd(new GSCmdAnimObj(GSDefinitions::CMD_ANIM_OBJ_REM_DONE, *this, cmdAnim.ObjID(), cmdAnim.ReleaseReq()));
        cmdAnim.RetSimConn().PostCommand(newCmd);
        break;
    }
    default:
        GSLogStream::LogError("GSAnimationThread - Unexpected cmd: ") << cmd.GetCmdID();
        break;
    }
}

}