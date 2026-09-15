#pragma once

#include "../GSSimConnect.h"
#include "GSSimObj.h"
#include "../GSDefinitions.h"

namespace NS_GSLiveAirportMSFS
{

class GSLinerCones : public GSSimObj, public GSSimObj::IObjUpdate
{
public:

    using GSSimObj::GSSimObj;

    void OnSpawned(bool ok, GSSimObj& obj) override;
    void OnDespawned(bool ok, GSSimObj& obj) override;

    void SetFinalPositionAndState();

private:

    void OnCreated() override;
    void OnDespawning() override {}
    bool PreSpawn() override;

    size_t m_spawned = 0;
};
}