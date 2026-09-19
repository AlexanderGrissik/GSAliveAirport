#pragma once

#include "GSSimObj.h"

namespace NS_GSLiveAirportMSFS
{

class GSMarshaller : public GSSimObj
{
public:

    using GSSimObj::GSSimObj;

private:

    bool PreSpawn() override;
    bool OnCreated() override;
    void OnArrived() override {}
    void OnDespawning() override {}
    
    bool m_hasAnimMarshall = true;
};
}
