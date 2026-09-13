#pragma once

#include "../GSSimConnect.h"
#include "GSSimObj.h"
#include "../GSDefinitions.h"

namespace NS_GSLiveAirportMSFS
{

class GSGroundPowerMSFS : public GSSimObj
{
public:

    #pragma pack(push, 1)
    struct StateWireDataSet
    {
        float hoseDeployed;
    };
    #pragma pack(pop)

    using GSSimObj::GSSimObj;

    static void InitDatums(GSSimConnect& handler);
    void SetFinalPositionAndState();

private:

    void OnCreated() override;
    bool PreSpawn() override;

    class GSReqSetDataSimObj : public GSSimObj::GSReqSetDataSimObj<StateWireDataSet, 1> {
    public:
        GSReqSetDataSimObj(GSSimConnect& simHandle, GSSimObj& simObj, std::array<StateWireDataSet, 1>&& data): 
            GSSimObj::GSReqSetDataSimObj<StateWireDataSet, 1>(
                simHandle, simObj, GSDefinitions::GSDefID::GSDefID_GroundPowerStateSet,
                std::forward<std::array<StateWireDataSet, 1>>(data)) {}
    };
};
}