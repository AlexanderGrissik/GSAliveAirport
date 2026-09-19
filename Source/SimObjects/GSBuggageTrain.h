#pragma once

#include "GSSimObj.h"
#include "GSBuggageLoader.h"
#include <cstddef>

namespace NS_GSLiveAirportMSFS
{

class GSBuggageTrain : public GSSimObj
{
public:

    GSBuggageTrain(GSSimConnect& simHandle, GSAircraft& aircraft, GSSimObjUpdate& parent, GSBuggageLoader& loader) :
        GSSimObj(simHandle, aircraft, parent), m_loader(loader) {}

    bool PreSpawn() override;

private:
        
    bool OnCreated() override;
    void OnArrived() override {}
    void OnDespawning() override {}

    GSBuggageLoader& m_loader;
};
}
