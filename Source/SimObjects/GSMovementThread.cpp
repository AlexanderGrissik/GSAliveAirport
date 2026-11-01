// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSMovementThread.h"
#include "Requests/GSReqMvmntObjPos.h"
#include "General/GSLogStream.h"
#include "Commands/GSCmdSimObj.h"
#include "General/GSGeography.h"

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

        if (IsSimStarted() && !m_inflightReqs) {
            if (m_trackObjs.size() && ((std::chrono::steady_clock::now() - m_lastTrackTime) > 1s))
                RequestTracking();
        }

        std::this_thread::sleep_for(m_trackObjs.size() ? 500ms : 2s);
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
        if (!obj.second.m_inProgress) {
            obj.second.m_inProgress = true;
            PostReqCommand(new GSReqMvmntObjPos(*this, *obj.second.m_simObj, &obj.second));
            IncrInflight();
        }
    }

    m_lastTrackTime = std::chrono::steady_clock::now();
}

void GSMovementThread::OnCommand(GSCommand& cmd)
{
    switch (cmd.GetCmdID()) {
    case GS_CMD_MVMNT_OBJ_ADD: {
        auto& cmdAnim = static_cast<GSCmdSimObj&>(cmd);
        m_trackObjs.try_emplace(cmdAnim.SimObj().GetSimObjectID(), &cmdAnim.SimObj(), std::chrono::steady_clock::now(), GSCoord{0.0, 0.0}, false, false);
        break;
    }
    case GS_CMD_MVMNT_OBJ_QUICK: {
        auto& cmdObj = static_cast<GSCmdSimObj&>(cmd);
        auto itr = m_trackObjs.find(cmdObj.ObjID());
        if (itr != m_trackObjs.end()) {
            itr->second.m_quick = true;
            if (!itr->second.m_inProgress) {
                Arrived(itr->second);
            }
        }
        break;
    }
    default:
        GSLogStream::LogError("GSAnimationThread - Unexpected cmd: ") << cmd.GetCmdID();
        break;
    }
}

void GSMovementThread::Arrived(GSMovementThread::TrackState& track)
{
    auto simObj = track.m_simObj;
    m_trackObjs.erase(track.m_simObj->GetSimObjectID());
    simObj->GetSimConnect().PostCommand(new GSCmdSimObj(GS_CMD_MVMNT_OBJ_ARR, *simObj));
}

void GSMovementThread::OnErrorPosReq(GSMovementThread::TrackState& track)
{
    track.m_inProgress = false;
    DecrInflight();
    Arrived(track);
}

void GSMovementThread::HandlePosMessage(SIMCONNECT_RECV_SIMOBJECT_DATA& entry, GSMovementThread::TrackState& track)
{
    static double ArrivalDistanceMtr = 2.0;

    track.m_inProgress = false;
    DecrInflight();
    if (track.m_quick) {
        Arrived(track);
        return;
    }

    StateWireDataGet rawStateGet;
    GSSimConnect::ReadMsgData(&rawStateGet, 0, sizeof(rawStateGet), entry);

    const auto nowTime = std::chrono::steady_clock::now();
    const GSCoord currentPos{rawStateGet.posLong, rawStateGet.posLat};
    const auto& dst = track.m_simObj->GetDestPoint();
    const auto distToDest = GSGeography::DistanceMeters({ dst.Longitude, dst.Latitude }, currentPos);

    bool arrived = false;
    bool maxTime = (nowTime - track.m_simObj->GetStartMoveTime() > 900s);
    if (maxTime || (ArrivalDistanceMtr > distToDest)) {
            arrived = true;
    } else if (nowTime - track.m_lastPosCheckTime > 10s) {
        if (GSGeography::DistanceMeters(track.m_lastPosition, currentPos) < 5.0)
            arrived = true;

        track.m_lastPosCheckTime = nowTime;
        track.m_lastPosition = currentPos;
    }
    
    if (arrived) {
        if (!maxTime && track.m_simObj->IsRevArrive()) {
            track.m_simObj->ShootWaypointsRev();

            track.m_lastPosCheckTime = nowTime;
            track.m_lastPosition = currentPos;
        } else
            Arrived(track);
    }
}


}