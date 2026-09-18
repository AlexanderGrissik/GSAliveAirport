#pragma once

#include "GSSimObj.h"
#include "GSBuggageLoader.h"
#include <cstddef>

namespace NS_GSLiveAirportMSFS
{

class GSBuggageTrain : public GSSimObj, public GSSimObj::IObjUpdate
{
public:

    GSBuggageTrain(GSSimConnect& simHandle, GSAircraft& aircraft, IObjUpdate& iUpdate, GSBuggageLoader& loader) :
        GSSimObj(simHandle, aircraft, iUpdate), m_loader(loader) {}

    void OnSpawned(bool ok, GSSimObj& obj) override;
    void OnDespawned(bool ok, GSSimObj& obj) override;
    bool PreSpawn() override;

    void SetFinalPositionAndState();

private:

    void OnCreated() override;
    void OnDespawning() override {}

    size_t m_spawned = 0;
    GSBuggageLoader& m_loader;
};
}
