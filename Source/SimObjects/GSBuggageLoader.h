#pragma once

#include "GSSimObj.h"
#include "../General/GSDefinitions.h"
#include "../General/GSLogStream.h"
#include <array>
#include <cstddef>
#include <utility>

namespace NS_GSLiveAirportMSFS
{

class GSBuggageLoader : public GSSimObj
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

    GSBuggageLoader(GSSimConnect& simHandle, GSAircraft& aircraft, GSSimObjUpdate& parent, bool front) :
        GSSimObj(simHandle, aircraft, parent), m_front(front) {}

    static void InitDatums(GSSimConnect& handler);

    void SetFinalPositionAndState(SIMCONNECT_RECV_SIMOBJECT_DATA& entry);
    void SetFinalPositionAndStatePost();

private:

    bool PreSpawn() override;
    bool OnCreated() override;
    void OnArrived() override;
    void OnDespawning() override;
    
    float CalcLoaderOpenAngle(const GSBuggageLoader::StateWireDataGet& rawStateGet);

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
            } else {
                GSLogStream::LogError("GSBuggageLoader::GSReqGetDataSimObj::OnMessage Unexpected Message: ") << message->dwID;
                m_simObj.OnObjSpawned(true);
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
