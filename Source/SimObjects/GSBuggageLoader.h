#pragma once

#include "../General/GSSimConnect.h"
#include "GSSimObj.h"
#include "../General/GSDefinitions.h"

namespace NS_GSLiveAirportMSFS
{

class GSBuggageLoader : public GSSimObj, public GSSimObj::IObjUpdate
{
public:

    #pragma pack(push, 1)
    struct StateWireDataGet
    {
        float endRampYFeet;
        float endRampZMeters;
        float pivotYFeet;
        float pivotZMeters;
    };
    struct StateWireDataSet
    {
        float targetAngleDegrees;
    };
    #pragma pack(pop)

    GSBuggageLoader(GSSimConnect& simHandle, GSAircraft& aircraft, IObjUpdate& iUpdate, bool front) :
        GSSimObj(simHandle, aircraft, iUpdate), m_front(front) {}

    static void InitDatums(GSSimConnect& handler);

    void OnSpawned(bool ok, GSSimObj& obj) override;
    void OnDespawned(bool ok, GSSimObj& obj) override;

    void SetFinalPositionAndState(SIMCONNECT_RECV_SIMOBJECT_DATA& entry);

private:

    void OnCreated() override;
    void OnDespawning() override;
    bool PreSpawn() override;

    float CalcLoaderOpenAngle(const GSBuggageLoader::StateWireDataGet& rawStateGet);

    size_t m_spawned = 0;
    float m_doorPosElev = 0.0f;
    DWORD m_doorIdx = 0;
    bool m_front = false;
    bool m_ext = true;

    class GSReqGetDataSimObj : public GSSimObj::GSReqGetDataSimObj {
    public:
        GSReqGetDataSimObj(GSSimConnect& simHandle, GSSimObj& simObj) :
            GSSimObj::GSReqGetDataSimObj(simHandle, simObj, GSDefinitions::GSDefID::GSDefID_BuggageLoaderExtStateGet) {}

        bool OnMessage(SIMCONNECT_RECV* message, DWORD messageSize) override {
            (void)messageSize;
            if (message->dwID == SIMCONNECT_RECV_ID_SIMOBJECT_DATA) {
                static_cast<GSBuggageLoader&>(m_simObj).SetFinalPositionAndState(*static_cast<SIMCONNECT_RECV_SIMOBJECT_DATA*>(message));
            }
            else {
                GSLogStream::LogError("GSBuggageLoader::GSReqGetDataSimObj::OnMessage Unexpected Message: ") << message->dwID;
            }
            return true;
        }

        void OnException(SIMCONNECT_RECV_EXCEPTION* message) override {
            m_simObj.OnObjSpawned(true);
            GSLogStream::LogError("GSBuggageLoader::GSReqGetDataSimObj::OnException: ") << message->dwException << ", " << message->dwIndex;
        }
    };

    class GSReqSetDataSimObj : public GSSimObj::GSReqSetDataSimObj<StateWireDataSet, 1> {
    public:
        GSReqSetDataSimObj(GSSimConnect& simHandle, GSSimObj& simObj, std::array<StateWireDataSet, 1>&& data) :
            GSSimObj::GSReqSetDataSimObj<StateWireDataSet, 1>(
                simHandle, simObj, GSDefinitions::GSDefID::GSDefID_BuggageLoaderExtStateSet,
                std::forward<std::array<StateWireDataSet, 1>>(data)) {
        }
    };
};
}
