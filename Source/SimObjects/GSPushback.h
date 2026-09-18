#pragma once

#include "../GSSimConnect.h"
#include "GSSimObj.h"
#include "../GSDefinitions.h"

namespace NS_GSLiveAirportMSFS
{

class GSPushback : public GSSimObj, public GSSimObj::IObjUpdate
{
public:

    enum PushType {
        PUSH_DEFAULT,
        PUSH_SMALL,
        PUSH_XL
    };

    GSPushback(GSSimConnect& simHandle, GSAircraft& aircraft, IObjUpdate& iUpdate, PushType tp):
        GSSimObj(simHandle, aircraft, iUpdate), m_pushType(tp) {}

    void OnSpawned(bool ok, GSSimObj& obj) override;
    void OnDespawned(bool ok, GSSimObj& obj) override;

    void SetFinalPositionAndState();

private:

    void OnCreated() override;
    void OnDespawning() override {}
    bool PreSpawn() override;

    PushType m_pushType;
    bool m_driver = false;
};
}