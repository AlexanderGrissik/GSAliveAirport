// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSMovementThread.h"
#include "../Requests/GSReqMvmntObjPos.h"
#include "../General/GSLogStream.h"
#include "../Commands/GSCmdSimObj.h"
#include "../General/GSGeography.h"

namespace NS_GSAliveAirport
{
using namespace std::chrono_literals;

constexpr std::array GSDatums_PlanePos{
    GSDatum{"PLANE LONGITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64},
    GSDatum{"PLANE LATITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64}
};

void GSMovementThread::Start()
{
    if (m_thread.joinable()) return;
    m_thread = std::jthread(&GSMovementThread::MoveTrackLoop, this);
}

void GSMovementThread::Stop()
{
    if (!m_thread.joinable()) return;
    m_thread.request_stop();
    m_thread.join();
}

void GSMovementThread::MoveTrackLoop(std::stop_token stopToken, GSMovementThread* self)
{
    self->RunLoopMoveTracking(stopToken);
}

void GSMovementThread::RunLoopMoveTracking(std::stop_token stopToken)
{
    while (!stopToken.stop_requested()) {
        RunDispatch(stopToken);

        if (!m_inflightReqs) {
            RemovePending();
        }

        if (IsSimStarted() && !m_inflightReqs) {
            if (m_trackObjs.size() && ((std::chrono::steady_clock::now() - m_lastTrackTime) > 1s))
                RequestTracking();
        }

        std::this_thread::sleep_for(m_trackObjs.size() ? 500ms : 5s);
    }

    OnDisconnect();
    Disconnect();
}

void GSMovementThread::OnConnect()
{
    InvokeAddDatums(GSDatums_PlanePos, GSDatumID_PlanePosition);
}

void GSMovementThread::RequestTracking()
{
    auto currTime = std::chrono::steady_clock::now();
    for (auto& obj : m_trackObjs) {
        PostReqCommand(new GSReqMvmntObjPos(*this, *obj.second.m_simObj, &obj.second));
        IncrInflight();
    }

    m_lastTrackTime = std::chrono::steady_clock::now();
}

void GSMovementThread::OnCommand(GSCommand& cmd)
{
    switch (cmd.GetCmdID()) {
    case GS_CMD_MVMNT_OBJ_ADD: {
        auto& cmdAnim = static_cast<GSCmdSimObj&>(cmd);
        m_trackObjs.try_emplace(cmdAnim.SimObj().GetSimObjectID(), &cmdAnim.SimObj(), std::chrono::steady_clock::now(), 0.0);
        break;
    }
    case GS_CMD_MVMNT_OBJ_REM: {
        auto& cmdAnim = static_cast<GSCmdSimObj&>(cmd);
        m_pendRemObjs.emplace(cmdAnim.SimObj().GetSimObjectID(), &cmdAnim.SimObj());
        break;
    }
    default:
        GSLogStream::LogError("GSAnimationThread - Unexpected cmd: ") << cmd.GetCmdID();
        break;
    }
}

void GSMovementThread::RemovePending()
{
    while (!m_pendRemObjs.empty()) {
        auto itr = m_pendRemObjs.begin();
        GSSimObj* obj = itr->second;

        m_trackObjs.erase(itr->first);
        m_pendRemObjs.erase(itr);

        itr->second->GetSimConnect().PostCommand(new GSCmdSimObj(GS_CMD_MVMNT_OBJ_REM_RET, *obj));
    }
}

void GSMovementThread::HandlePosMessage(SIMCONNECT_RECV_SIMOBJECT_DATA& entry, GSMovementThread::TrackState& track)
{
    static double ArrivalDistanceMtr = 10.0;
    StateWireDataGet rawStateGet;
    GSSimConnect::ReadMsgData(&rawStateGet, 0, sizeof(rawStateGet), entry);

    const auto nowTime = std::chrono::steady_clock::now();
    const auto& dst = track.m_simObj->GetDestPoint();
    const auto distToDest = GSGeography::DistanceMeters({ dst.Longitude, dst.Latitude }, { rawStateGet.posLong, rawStateGet.posLat });

    bool arrived = false;
    if ((nowTime - track.m_simObj->GetStartMoveTime() > 900s) || (ArrivalDistanceMtr > distToDest)) {
        arrived = true;
    } else if (nowTime - track.m_lastPosCheckTime > 10s) {
        if (std::abs(distToDest - track.m_lastDistToDest) < 1.0) {
            arrived = true;
        }

        track.m_lastPosCheckTime = nowTime;
        track.m_lastDistToDest = distToDest;
    }

    if (arrived) {
        auto objID = track.m_simObj->GetSimObjectID();
        auto simObj = track.m_simObj;
        m_trackObjs.erase(objID);
        m_pendRemObjs.erase(objID);
        simObj->GetSimConnect().PostCommand(new GSCmdSimObj(GS_CMD_MVMNT_OBJ_ARR, *simObj));
    }
    
    DecrInflight();
}


}