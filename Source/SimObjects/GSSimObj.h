#pragma once

#include "../General/GSCoord.h"
#include "../General/GSLogStream.h"
#include "../General/GSRequest.h"
#include "../General/GSSimConnect.h"
#include "../Animation/GSAnimationObject.h"
#include "../General/GSRoadsNetwork.h"
#include "GSAircraft.h"
#include "GSSimObjUpdate.h"
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace NS_GSLiveAirportMSFS
{

class GSSimObj : public GSSimObjUpdate
{
public:

    class GSSimObjReq : public GSRequest {
    public:
        GSSimObjReq(GSSimConnect& simHandle, GSSimObj& simObj): GSRequest(simHandle), m_simObj(simObj) {}
    protected:
        GSSimObj& m_simObj;
    };

    GSSimObj(GSSimConnect& simHandle, GSAircraft& aircraft, GSSimObjUpdate& parent):
        m_simHandle(simHandle), m_aircraft(aircraft), m_parent(parent) {}
    virtual ~GSSimObj() {}

    static void InitDatums(GSSimConnect& handler);
    
    virtual bool PreSpawn() = 0;
    virtual void OnArrived() = 0;
    
    void OnSpawned(bool ok, GSSimObj& obj) override;
    void OnDespawned() override; 
    
    void Spawn();
    void Despawn();
    void SpawnAttached();
    void DespawnAttached();
    void Freeze();
    void RegisterAnim(GSAnimationObject* animObj);
    void UnregisterAnim();
    
    const std::string& GetTitle() const { return m_title; }
    const SIMCONNECT_DATA_INITPOSITION& GetInitPos() const { return m_initPos; }
    SIMCONNECT_DATA_INITPOSITION& GetInitPos() { return m_initPos; }
    SIMCONNECT_OBJECT_ID GetSimObjectID() const { return m_simObjectID; }
    GSSimConnect& GetSimConnect() const { return m_simHandle; }
    const SIMCONNECT_DATA_WAYPOINT& GetDestPoint() const { return m_aiWaypoints.back(); }

    void SetSimObjectID(DWORD id) { m_simObjectID = static_cast<SIMCONNECT_OBJECT_ID>(id); }
    void OnObjSpawned(bool ok) { m_parent.OnSpawned(ok, *this); }
    void OnObjDespawned() { m_parent.OnDespawned(); }
    void SetTitle(const std::string& title) { m_title = title; }
    void SetHeading(double heading) { m_initPos.Heading = heading; }
    void SetPosition(const GSCoord& pos) { m_initPos.Longitude = pos.Long(); m_initPos.Latitude = pos.Lat(); }
    void PrepareRoute();
    void FinalizeRoute();
    void ContinueSpawn();
    void RestoreInitPos();

    void AddWaypoint(const GSCoord& pos, float alt, float ktsSpeed, float percThrot, unsigned flags);
    void ShootWaypoints();

protected:

    virtual bool OnCreated() = 0;
    virtual void OnDespawning() = 0;
    
    GSSimConnect& m_simHandle;
    GSAircraft& m_aircraft;
    SIMCONNECT_OBJECT_ID m_simObjectID = 0;
    GSSimObjUpdate& m_parent;
    std::string m_title;
    SIMCONNECT_DATA_INITPOSITION m_initPos{};
    std::vector<std::unique_ptr<GSSimObj>> m_attached; 
    std::vector<SIMCONNECT_DATA_WAYPOINT> m_aiWaypoints;
    GSCoord m_preMoveInitPos;
    size_t m_childSpawned = 0;
    bool m_hasAnim = false;
    bool m_routePrepared = false;

    class GSReqCreate : public GSSimObjReq {
    public:
        using GSSimObjReq::GSSimObjReq;
        GSSimObjReq::SendResult Process() override;
        bool OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override;
        void OnException(SIMCONNECT_RECV_EXCEPTION *message) override;
    };

    class GSReqDelete : public GSSimObjReq {
    public:
        using GSSimObjReq::GSSimObjReq;
        GSRequest::SendResult Process() override;        
        bool OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override;
        void OnException(SIMCONNECT_RECV_EXCEPTION *message) override;
    };

    class GSReqGetDataSimObj : public GSSimObjReq {
    public:
        GSReqGetDataSimObj(GSSimConnect& simHandle, GSSimObj& simObj, SIMCONNECT_DATA_DEFINITION_ID definitionID):
            GSSimObjReq(simHandle, simObj), m_definitionID(definitionID) {}
        GSRequest::SendResult Process() override;
    private:
        SIMCONNECT_DATA_DEFINITION_ID m_definitionID;
    };

    template <typename T, DWORD N>
    class GSReqSetDataSimObj : public GSSimObjReq {
    public:
        GSReqSetDataSimObj(GSSimConnect& simHandle, GSSimObj& simObj, SIMCONNECT_DATA_DEFINITION_ID definitionID, std::array<T,N>&& data):
            GSSimObjReq(simHandle, simObj), m_definitionID(definitionID), m_data(std::forward<std::array<T,N>>(data)) {}
        GSRequest::SendResult Process() override;
        bool OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override { 
            (void)message;
            (void)messageSize;
            return true; 
        }
        void OnException(SIMCONNECT_RECV_EXCEPTION *message) override { (void)message; }
    private:
        SIMCONNECT_DATA_DEFINITION_ID m_definitionID;
        std::array<T,N> m_data;
    };

    class GSReqTxClientEvent : public GSSimObjReq {
    public:
        GSReqTxClientEvent(GSSimConnect& simHandle, GSSimObj& simObj, SIMCONNECT_CLIENT_EVENT_ID eventID, DWORD data):
            GSSimObjReq(simHandle, simObj), m_eventID(eventID), m_data(data) {}
        GSRequest::SendResult Process() override;
        bool OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override { 
            (void)message;
            (void)messageSize;
            return true; 
        }
        void OnException(SIMCONNECT_RECV_EXCEPTION *message) override { (void)message; }
    private:
        SIMCONNECT_CLIENT_EVENT_ID m_eventID;
        DWORD m_data;
    };

    class GSReqTxEventEx1 : public GSSimObjReq {
    public:
        GSReqTxEventEx1(
            GSSimConnect& simHandle, GSSimObj& simObj, SIMCONNECT_CLIENT_EVENT_ID eventID,
            SIMCONNECT_OBJECT_ID objID, DWORD data0, DWORD data1):
            GSSimObjReq(simHandle, simObj), m_eventID(eventID), m_data0(data0), m_data1(data1), m_objID(objID) {}
        GSRequest::SendResult Process() override;
        bool OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override { 
            (void)message;
            (void)messageSize;
            return true; 
        }
        void OnException(SIMCONNECT_RECV_EXCEPTION *message) override { (void)message; }
    private:
        SIMCONNECT_CLIENT_EVENT_ID m_eventID;
        DWORD m_data0;
        DWORD m_data1;
        SIMCONNECT_OBJECT_ID m_objID;
    };

    class GSReqSetPos : public GSReqSetDataSimObj<SIMCONNECT_DATA_INITPOSITION, 1> {
    public:
        GSReqSetPos(
            GSSimConnect& simHandle, GSSimObj& simObj, SIMCONNECT_DATA_DEFINITION_ID definitionID,
            std::array<SIMCONNECT_DATA_INITPOSITION, 1>&& data) :
            GSSimObj::GSReqSetDataSimObj<SIMCONNECT_DATA_INITPOSITION, 1>(
                simHandle, simObj, definitionID, std::forward<std::array<SIMCONNECT_DATA_INITPOSITION, 1>>(data)) {
        }
    };

    class GSReqRemAnim : public GSSimObjReq {
        using GSSimObjReq::GSSimObjReq;
        GSRequest::SendResult Process() override { m_simObj.Despawn(); return { {0,0,NOERROR},false }; }
        bool OnMessage(SIMCONNECT_RECV* message, DWORD messageSize) override { VD(message); VD(messageSize); return true; };
        void OnException(SIMCONNECT_RECV_EXCEPTION* message) override { VD(message); };
    };

    friend GSReqCreate;
};

template <typename T, DWORD N>
GSRequest::SendResult GSSimObj::GSReqSetDataSimObj<T,N>::Process()
{
    auto simRC = m_simHandle.Invoke(
        SimConnect_SetDataOnSimObject, m_definitionID, m_simObj.GetSimObjectID(), 0, N, static_cast<DWORD>(sizeof(T)), m_data.data());
    if (!simRC.isOK()) {
        GSLogStream::LogError("GSSimObjReq::GSReqSetDataSimObj::Process Failed call: ") << simRC.rc;
    }

    return {simRC, false};
}

}
