#pragma once

#include "../GSSimConnect.h"
#include "GSSimObj.h"
#include "../GSDefinitions.h"

namespace NS_GSLiveAirportMSFS
{

class GSGroundPower : public GSSimObj, public GSSimObj::IObjUpdate
{
public:

    enum GPUType {
        GPU_DEFAULT,
        GPU_MEDIUM,
        GPU_LARGE
    };

    #pragma pack(push, 1)
    struct StateWireDataSet
    {
        float hoseDeployed;
    };
    #pragma pack(pop)

    GSGroundPower(GSSimConnect& simHandle, GSAircraft& aircraft, IObjUpdate& iUpdate, GPUType tp) :
        GSSimObj(simHandle, aircraft, iUpdate), m_gpuType(tp) {
    }

    static void InitDatums(GSSimConnect& handler);

    void OnSpawned(bool ok, GSSimObj& obj) override;
    void OnDespawned(bool ok, GSSimObj& obj) override;

    void SetFinalPositionAndState();

private:

    void OnCreated() override;
    void OnDespawning() override {}
    bool PreSpawn() override;

    GPUType m_gpuType;

    class GSReqSetDataSimObj : public GSSimObj::GSReqSetDataSimObj<StateWireDataSet, 1> {
    public:
        GSReqSetDataSimObj(
            GSSimConnect& simHandle, GSSimObj& simObj, SIMCONNECT_DATA_DEFINITION_ID definitionID, 
            std::array<StateWireDataSet, 1>&& data): 
            GSSimObj::GSReqSetDataSimObj<StateWireDataSet, 1>(
                simHandle, simObj, definitionID, std::forward<std::array<StateWireDataSet, 1>>(data)) {}
    };
};
}