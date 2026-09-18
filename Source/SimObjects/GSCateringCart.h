#pragma once

#include "GSSimObj.h"
#include "../General/GSDefinitions.h"
#include "../General/GSLogStream.h"
#include <array>
#include <utility>

namespace NS_GSLiveAirportMSFS
{

class GSCateringCart : public GSSimObj
{
public:

    #pragma pack(push, 1)
    struct StateWireDataGet
    {
        float doorContactOffsetZ;
    };
    struct StateWireDataSet
    {
        double posLong;
        double posLat;
        float elevationTarget;
        float openingTarget;
    };
    #pragma pack(pop)

    using GSSimObj::GSSimObj;

    static void InitDatums(GSSimConnect& handler);
    void SetFinalPositionAndState(SIMCONNECT_RECV_SIMOBJECT_DATA& entry);

private:

    void OnCreated() override;
    void OnDespawning() override {}
    bool PreSpawn() override;

    float m_doorPosElev = 0.0f;

    class GSReqGetDataSimObj : public GSSimObj::GSReqGetDataSimObj {
    public:
        GSReqGetDataSimObj(GSSimConnect& simHandle, GSSimObj& simObj):
            GSSimObj::GSReqGetDataSimObj(simHandle, simObj, GSDefinitions::GSDefID::GSDefID_CateringTruckStateGet) {}

        bool OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override {
            (void)messageSize;
            if (message->dwID == SIMCONNECT_RECV_ID_SIMOBJECT_DATA) {
                static_cast<GSCateringCart&>(m_simObj).SetFinalPositionAndState(*static_cast<SIMCONNECT_RECV_SIMOBJECT_DATA*>(message));
            } else {
                GSLogStream::LogError("GSCateringCart::GSReqGetDataSimObj::OnMessage Unexpected Message: ") << message->dwID;
            }
            m_simObj.OnObjSpawned(true);
            return true;
        }

        void OnException(SIMCONNECT_RECV_EXCEPTION *message) override {
            m_simObj.OnObjSpawned(true);
            GSLogStream::LogError("GSCateringCart::GSReqGetDataSimObj::OnException: ") << message->dwException << ", " << message->dwIndex;
        }
    };

    class GSReqSetDataSimObj : public GSSimObj::GSReqSetDataSimObj<StateWireDataSet, 1> {
    public:
        GSReqSetDataSimObj(GSSimConnect& simHandle, GSSimObj& simObj, std::array<StateWireDataSet, 1>&& data): 
            GSSimObj::GSReqSetDataSimObj<StateWireDataSet, 1>(
                simHandle, simObj, GSDefinitions::GSDefID::GSDefID_CateringTruckStateSet,
                std::forward<std::array<StateWireDataSet, 1>>(data)) {}
    };
};
}
