#pragma once

#include "GSSimObj.h"
#include "../General/GSCoord.h"

namespace NS_GSLiveAirportMSFS
{

class GSStandingHuman : public GSSimObj
{
public:

    enum HumanType {
        PILOT,
        PASSENGER
    };

    GSStandingHuman(GSSimConnect& simHandle, GSAircraft& aircraft, IObjUpdate& iUpdate, HumanType humanType, const GSCoord& loc):
        GSSimObj(simHandle, aircraft, iUpdate), m_humanType(humanType), m_loc(loc){}

    void SetFinalPositionAndState();
    bool PreSpawn() override;

private:

    void OnCreated() override;
    void OnDespawning() override {}

    HumanType m_humanType = PASSENGER;
    GSCoord m_loc;
};
}
