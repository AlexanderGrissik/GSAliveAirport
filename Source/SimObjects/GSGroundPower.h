#pragma once

#include "GSSimObj.h"
#include <array>
#include <utility>

namespace NS_GSLiveAirportMSFS
{

class GSGroundPower : public GSSimObj
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

    GSGroundPower(GSSimConnect& simHandle, GSAircraft& aircraft, GSSimObjUpdate& parent, GPUType tp) :
        GSSimObj(simHandle, aircraft, parent), m_gpuType(tp) {
    }

    static void InitDatums(GSSimConnect& handler);

private:

    bool PreSpawn() override;
    bool OnCreated() override;
    void OnArrived() override;
    void OnDespawning() override {}
    
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
