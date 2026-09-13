#pragma once

#include "GSSimObj.h"
#include "../GSCoord.h"

namespace NS_GSLiveAirportMSFS
{

class GSStatic : public GSSimObj
{
public:

    enum PosRelation {
        REL_ABSOLUTE,
        REL_AIRCRAFT
    };

    using GSSimObj::GSSimObj;

    void SetFinalPositionAndState();
    void SetPosRel(PosRelation rel) { m_posRel = rel; }

private:

    void OnCreated() override;
    bool PreSpawn() override;

    PosRelation m_posRel = REL_AIRCRAFT;
};
}