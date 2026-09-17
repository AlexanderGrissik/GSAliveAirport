#pragma once

#include "../GSSimConnect.h"
#include "GSSimObj.h"
#include "../GSDefinitions.h"

namespace NS_GSLiveAirportMSFS
{

class GSWingmans : public GSSimObj, public GSSimObj::IObjUpdate
{
public:

    enum CountType{
        AIRLINE,
        SMALL
    };

    GSWingmans(GSSimConnect& simHandle, GSAircraft& aircraft, IObjUpdate& iUpdate, CountType tp):
        GSSimObj(simHandle, aircraft, iUpdate), m_countTP(tp) {}

    void OnSpawned(bool ok, GSSimObj& obj) override;
    void OnDespawned(bool ok, GSSimObj& obj) override;

    void SetFinalPositionAndState();

private:

    void OnCreated() override;
    void OnDespawning() override {}
    bool PreSpawn() override;

    CountType m_countTP;
    size_t m_spawned = 0;
};
}