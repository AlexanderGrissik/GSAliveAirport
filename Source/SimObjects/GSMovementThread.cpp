#include "GSMovementThread.h"
#include "../General/GSLogStream.h"
#include "../Commands/GSCmdSimObj.h"
#include "../General/GSGeography.h"

namespace NS_GSLiveAirportMSFS
{
using namespace std::chrono_literals;

constexpr std::array GSDatums_PlanePos{
    GSDefinitions::DatumSpec{"PLANE LONGITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64},
    GSDefinitions::DatumSpec{"PLANE LATITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64}
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
    InvokeAddDatums(GSDatums_PlanePos, GSDefinitions::GSDefID_PlanePosition);
}

void GSMovementThread::RequestTracking()
{
    auto currTime = std::chrono::steady_clock::now();
    for (auto obj : m_trackObjs) {
        PostReqCommand(new GSReqObjPosition(*this, *obj.second));
        IncrInflight();
    }

    m_lastTrackTime = std::chrono::steady_clock::now();
}

void GSMovementThread::OnCommand(GSCommand& cmd)
{
    switch (cmd.GetCmdID()) {
    case GSDefinitions::CMD_MVMNT_OBJ_ADD: {
        auto& cmdAnim = static_cast<GSCmdSimObj&>(cmd);
        m_trackObjs.emplace(cmdAnim.SimObj().GetSimObjectID(), &cmdAnim.SimObj());
        break;
    }
    case GSDefinitions::CMD_MVMNT_OBJ_REM: {
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

        CmdPtr cmd(new GSCmdSimObj(GSDefinitions::CMD_MVMNT_OBJ_REM_RET, *obj));
        itr->second->GetSimConnect().PostCommand(cmd);
    }
}

void GSMovementThread::HandlePosMessage(SIMCONNECT_RECV_SIMOBJECT_DATA& entry, GSSimObj& obj)
{
    static double ArrivalDistanceMtr = 10.0;
    StateWireDataGet rawStateGet;
    GSSimConnect::ReadMsgData(&rawStateGet, sizeof(rawStateGet), entry);

    const auto& dst = obj.GetDestPoint();
    if ((std::chrono::steady_clock::now() - obj.GetStartMoveTime() > 300s) ||
        (ArrivalDistanceMtr > GSGeography::DistanceMeters({ dst.Longitude, dst.Latitude }, { rawStateGet.posLong, rawStateGet.posLat }))) {
        m_trackObjs.erase(obj.GetSimObjectID());
        m_pendRemObjs.erase(obj.GetSimObjectID());
        CmdPtr cmd(new GSCmdSimObj(GSDefinitions::CMD_MVMNT_OBJ_ARR, obj));
        obj.GetSimConnect().PostCommand(cmd);
    }
    
    DecrInflight();
}

GSRequest::SendResult GSMovementThread::GSReqObjPosition::Process()
{
    auto id = m_simHandle.NextRequestID();
    GSRequest::SendResult rc = {
        m_simHandle.InvokeRequest(id, SimConnect_RequestDataOnSimObject, id, GSDefinitions::GSDefID_PlanePosition,
            m_simObj.GetSimObjectID(), SIMCONNECT_PERIOD_ONCE, 0U, 0U, 0U, 0U), true };

    if (!rc.m_simRC.isOK()) {
        GSLogStream::LogError("GSMovementThread::GSReqObjPosition::Process Failed call: ") << rc.m_simRC.rc;
        rc.m_keep = false;
    }

    return rc;
}

bool GSMovementThread::GSReqObjPosition::OnMessage(SIMCONNECT_RECV* message, DWORD messageSize)
{
    GSMovementThread& tracker = static_cast<GSMovementThread&>(m_simHandle);

    if (!message || message->dwID != SIMCONNECT_RECV_ID_SIMOBJECT_DATA) {
        GSLogStream::LogError("Unexpected message: ") << (message ? message->dwID : -1) << ", Size: " << messageSize;
    } else {
        tracker.HandlePosMessage(*reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA*>(message), m_simObj);
    }

    return true;
}

void GSMovementThread::GSReqObjPosition::OnException(SIMCONNECT_RECV_EXCEPTION* message)
{
    GSMovementThread& tracker = static_cast<GSMovementThread&>(m_simHandle);

    GSLogStream::LogError("GSMovementThread::GSReqObjPosition:: Exception: ") << message->dwID <<
        ", Exception: " << message->dwException << "Index:" << message->dwIndex;

    tracker.DecrInflight();
}

}