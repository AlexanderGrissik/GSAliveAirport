#pragma once

#include "GSSimObj.h"

namespace NS_GSLiveAirportMSFS
{

class GSMarshaller : public GSSimObj
{
public:

    using GSSimObj::GSSimObj;

    void SetFinalPositionAndState();

private:

    void OnCreated() override;
    void OnDespawning() override {}
    bool PreSpawn() override;

    bool m_hasAnimMarshall = true;
};
}
