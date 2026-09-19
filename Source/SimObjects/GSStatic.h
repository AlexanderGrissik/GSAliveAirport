#pragma once

#include "GSSimObj.h"

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

    void SetPosRel(PosRelation rel) { m_posRel = rel; }
    void SetAnimObj(GSAnimationObject* animObj) { m_animObj.reset(animObj); }
    bool PreSpawn() override;

private:

    bool OnCreated() override;
    void OnArrived() override {}
    void OnDespawning() override {}

    PosRelation m_posRel = REL_AIRCRAFT;
    std::unique_ptr<GSAnimationObject> m_animObj;
};
}
