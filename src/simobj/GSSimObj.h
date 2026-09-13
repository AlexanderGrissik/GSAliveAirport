#pragma once

#include "../GSLogStream.h"
#include "GSAircraft.h"
#include "../GSRequest.h"
#include <vector>
#include <memory>

namespace NS_GSLiveAirportMSFS
{

class GSSimConnect;

class GSSimObj
{
public:

    struct IObjUpdate {
        virtual void OnSpawned(bool ok, GSSimObj& obj) = 0;
        virtual void OnDespawned(bool ok, GSSimObj& obj) = 0;
    };

    class GSSimObjReq : public GSRequest {
    public:
        GSSimObjReq(GSSimConnect& simHandle, GSSimObj& simObj): GSRequest(simHandle), m_simObj(simObj) {}
    protected:
        GSSimObj& m_simObj;
    };

    GSSimObj(GSSimConnect& simHandle, GSAircraft& aircraft, IObjUpdate& iUpdate): m_simHandle(simHandle), m_aircraft(aircraft), m_iUpdate(iUpdate) {}
    virtual ~GSSimObj() {}

    static void InitDatums(GSSimConnect& handler);
    void Spawn();
    void Despawn();
    void SpawnAttached();
    virtual bool PreSpawn() = 0;

    const std::string& GetTitle() const { return m_title; }
    const SIMCONNECT_DATA_INITPOSITION& GetInitPos() const { return m_initPos; }
    SIMCONNECT_OBJECT_ID GetSimObjectID() const { return m_simObjectID; }

    void SetSimObjectID(DWORD id) { m_simObjectID = static_cast<SIMCONNECT_OBJECT_ID>(id); }
    void OnSpawned(bool ok) { m_iUpdate.OnSpawned(ok, *this); }
    void OnDespawned(bool ok) { m_iUpdate.OnDespawned(ok, *this); }

protected:

    virtual void OnCreated() = 0;

    GSSimConnect& m_simHandle;
    GSAircraft& m_aircraft;
    SIMCONNECT_OBJECT_ID m_simObjectID = 0;
    IObjUpdate& m_iUpdate;
    std::string m_title;
    SIMCONNECT_DATA_INITPOSITION m_initPos{};
    std::vector<std::unique_ptr<GSSimObj>> m_attached; 

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
