#include "GSWingmans.h"
#include "../GSGeography.h"
#include "../GSCatalog.h"
#include "GSStatic.h"
#include "Animation/GSAnimSingle.h"

namespace NS_GSLiveAirportMSFS
{

void GSWingmans::OnCreated()
{
    SetFinalPositionAndState();
    if (m_attached.size())
        SpawnAttached();
    else
        OnObjSpawned(true);
}

void GSWingmans::OnSpawned(bool ok, GSSimObj& obj)
{
    (void)obj; (void)ok;
    if (++m_spawned == m_attached.size())
        OnObjSpawned(true);
}

void GSWingmans::OnDespawned(bool ok, GSSimObj& obj)
{
    (void)obj; (void)ok;
    if (--m_spawned <= 0) {
        m_attached.clear();
        Despawn();
    }
}

bool GSWingmans::PreSpawn()
{
    const auto& airData = m_aircraft.GetRawData();

    m_title = GSCatalog::GetInstance().GetWingwalker();
    if (m_title.empty())
        return false;

    m_initPos.Heading = airData.headingDegrees;
    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    m_initPos.OnGround = 1;
    m_initPos.Altitude = airData.groundAltitudeFeet;

    double YShift = (m_countTP == GSWingmans::AIRLINE ? 8 : 5);
    auto halfWing = airData.wingSpanMeters / 2.0;
    const auto conePos = GSGeography::RelativePosition(
        airData.headingDegrees, m_aircraft.GetLongLat(), -halfWing - YShift, 0);
    m_initPos.Longitude = conePos.Long();
    m_initPos.Latitude = conePos.Lat();

    if (m_countTP == GSWingmans::AIRLINE) {
        auto ptrWalker2 = new GSStatic(m_simHandle, m_aircraft, *this);
        ptrWalker2->SetTitle(m_title);
        ptrWalker2->SetPosition({ halfWing, 0 });
        ptrWalker2->PreSpawn();

        auto* animWorker2 = new GSAnimSingle(0);
        animWorker2->AddFrameSet(GSDefinitions::GSDefID_AnimVelocBodyY, 230, 267, 30.0);
        ptrWalker2->SetAnimObj(animWorker2);

        m_attached.emplace_back(ptrWalker2);

        auto ptrWalker3 = new GSStatic(m_simHandle, m_aircraft, *this);
        ptrWalker3->SetTitle(m_title);
        ptrWalker3->SetPosition({ -halfWing, 0 });
        ptrWalker3->PreSpawn();

        auto* animWorker3 = new GSAnimSingle(0);
        animWorker3->AddFrameSet(GSDefinitions::GSDefID_AnimVelocBodyY, 230, 267, 30.0);
        ptrWalker3->SetAnimObj(animWorker3);

        m_attached.emplace_back(ptrWalker3);
    }

    return true;
}

void GSWingmans::SetFinalPositionAndState()
{
    Freeze();

    auto* anim = new GSAnimSingle(m_simObjectID);
    anim->AddFrameSet(GSDefinitions::GSDefID_AnimVelocBodyY, 230, 267, 30.0);
    RegisterAnim(anim);
}
}
