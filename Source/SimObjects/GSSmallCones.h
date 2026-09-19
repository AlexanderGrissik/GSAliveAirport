#pragma once

#include "GSSimObj.h"
#include <cstddef>

namespace NS_GSLiveAirportMSFS
{

class GSSmallCones : public GSSimObj
{
public:

    using GSSimObj::GSSimObj;

private:

    bool PreSpawn() override;
    bool OnCreated() override;
    void OnArrived() override {}
    void OnDespawning() override {}
};
}
