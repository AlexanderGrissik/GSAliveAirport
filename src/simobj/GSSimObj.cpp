#include "GSSimObj.h"
#include "../cmds/GSCmdReq.h"
#include "../GSSimConnect.h"

namespace NS_GSLiveAirportMSFS
{

void GSSimObj::InitDatums(GSSimConnect& handler)
{
    handler.InvokeMapClientEvent(GSDefinitions::GSDefID_Freeze_LongLat, "FREEZE_LATITUDE_LONGITUDE_SET");
    handler.InvokeMapClientEvent(GSDefinitions::GSDefID_Freeze_Altitude, "FREEZE_ALTITUDE_SET");
    handler.InvokeMapClientEvent(GSDefinitions::GSDefID_Freeze_Attitude, "FREEZE_ATTITUDE_SET");
}

void GSSimObj::Spawn()
{
    m_simHandle.PostReqCommand(new GSReqCreate(m_simHandle, *this));
}

void GSSimObj::Despawn()
{
    m_simHandle.PostReqCommand(new GSReqDelete(m_simHandle, *this));

    for (auto& obj : m_attached)
        obj->Despawn();
}

void GSSimObj::SpawnAttached()
{
    for (auto& obj : m_attached)
        obj->Spawn();
}

GSRequest::SendResult GSSimObj::GSReqCreate::Process()
{
    auto id = m_simHandle.NextRequestID();
    GSSimObjReq::SendResult rc = {
        m_simHandle.InvokeRequest(id, SimConnect_AICreateSimulatedObject_EX1, m_simObj.GetTitle().c_str(), "", m_simObj.GetInitPos(), id), true };
    
    if (!rc.m_simRC.isOK()) {
        GSLogStream::LogError("GSSimObjReq::GSReqCreate::Process Failed call: ") << rc.m_simRC.rc;
        rc.m_keep = false;
        m_simObj.OnSpawned(false);
    } 

    return rc;
}

bool GSSimObj::GSReqCreate::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    if (message->dwID == SIMCONNECT_RECV_ID_ASSIGNED_OBJECT_ID) {
        auto* msg = static_cast<SIMCONNECT_RECV_ASSIGNED_OBJECT_ID*>(message);
        m_simObj.SetSimObjectID(msg->dwObjectID);
        m_simObj.OnCreated();
    } else {
        m_simObj.OnSpawned(false);
        GSLogStream::LogError("GSSimObjReq::GSReqCreate::OnMessage Unexpected Message: ") << message->dwID;
    }
    (void)messageSize;
    return true;
}

void GSSimObj::GSReqCreate::OnException(SIMCONNECT_RECV_EXCEPTION *message)
{
    m_simObj.OnSpawned(false);
    GSLogStream::LogError("GSSimObjReq::GSReqCreate::OnException: ") << message->dwException << ", " << message->dwIndex;
}

GSRequest::SendResult GSSimObj::GSReqDelete::Process() 
{
    auto id = m_simHandle.NextRequestID();
    auto simRC = m_simHandle.Invoke(SimConnect_AIRemoveObject, m_simObj.GetSimObjectID(), id);
    if (!simRC.isOK()) {
        GSLogStream::LogError("GSSimObjReq::GSReqDelete::Process Failed call: ") << simRC.rc;
    } else {
        m_simObj.OnDespawned(true);
        //m_simObj.SetInProgress(true); // Unclear if SimConnect_AIRemoveObject fires OnMessage.
    }
    return {simRC, true};
}

bool GSSimObj::GSReqDelete::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    (void)message;
    (void)messageSize;
    return true;
}

void GSSimObj::GSReqDelete::OnException(SIMCONNECT_RECV_EXCEPTION *message)
{
    (void)message;
}

GSRequest::SendResult GSSimObj::GSReqGetDataSimObj::Process()
{
    auto id = m_simHandle.NextRequestID();
    GSSimObjReq::SendResult rc = {
        m_simHandle.InvokeRequest(
            id, SimConnect_RequestDataOnSimObject, id, m_definitionID, m_simObj.GetSimObjectID(), SIMCONNECT_PERIOD_ONCE, 0, 0, 0, 0), true };
    
    if (!rc.m_simRC.isOK()) {
        GSLogStream::LogError("GSSimObjReq::GSReqGetDataSimObj::Process Failed call: ") << rc.m_simRC.rc;
        rc.m_keep = false;
        m_simObj.OnSpawned(false);
    }

    return rc;
}

GSRequest::SendResult GSSimObj::GSReqTxClientEvent::Process()
{
    auto simRC = m_simHandle.Invoke(
        SimConnect_TransmitClientEvent, m_simObj.GetSimObjectID(), m_eventID, m_data, 
        SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
    if (!simRC.isOK()) {
        GSLogStream::LogError("GSSimObjReq::GSReqTxClientEvent::Process Failed call: ") << simRC.rc;
    }

    return {simRC, false};
}

}