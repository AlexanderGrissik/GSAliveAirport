#pragma once

#include "../GSSimConnect.h"
#include "GSSimObj.h"
#include "../GSDefinitions.h"

namespace NS_GSLiveAirportMSFS
{

class GSLavatoryTruck : public GSSimObj
{
public:

    using GSSimObj::GSSimObj;

    void SetFinalPositionAndState();

private:

    void OnCreated() override;
    void OnDespawning() override {}
    bool PreSpawn() override;
};
}