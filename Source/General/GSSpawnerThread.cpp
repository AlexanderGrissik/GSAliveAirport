// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSSpawnerThread.h"
#include "GSLogStream.h"
#include "../SimObjects/GSAircraftGroundSmall.h"
#include "../SimObjects/GSAircraftGroundMedium.h"
#include "../SimObjects/GSAircraftGroundLarge.h"
#include "../SimObjects/GSAircraftGroundXL.h"
#include "../SimObjects/GSCateringCart.h"
#include "../SimObjects/GSGroundPower.h"
#include "../SimObjects/GSBuggageLoader.h"
#include "../SimObjects/GSStaircase.h"
#include "../Commands/GSCmdAircraftUpdate.h"
#include "../Commands/GSCmdAircraftAdd.h"
#include "../Commands/GSCmdAnimObj.h"
#include "../Commands/GSCmdSimObj.h"
#include "../UI/GSAliveUIAPI.h"
#include "GSCatalog.h"
#include "GSGeography.h"
#include "GSRandom.h"
#include <chrono>
#include <array>
#include <algorithm>

using namespace std::chrono_literals;

namespace NS_GSAliveAirport
{

void GSSpawnerThread::Start()
{
    if (m_thread.joinable()) return;
    m_thread = std::jthread([this](std::stop_token stopToken) {
        while (!stopToken.stop_requested()) {
            RunDispatch(stopToken);
            CheckForPendingRemove();
            CheckForPendingAdd();

            if (!IsSimStarted() || !IsLastLoopMessage()) {
                std::this_thread::sleep_for(1000ms);
            } else {
                std::this_thread::sleep_for(50ms);
            }
        }
        OnDisconnect();
        Disconnect();
    });
}

void GSSpawnerThread::Stop()
{
    if (!m_thread.joinable()) return;
    m_thread.request_stop();
    m_thread.join();
}

void GSSpawnerThread::OnConnect() 
{
    GSSimObj::InitDatums(*this);
    GSCateringCart::InitDatums(*this); 
    GSGroundPower::InitDatums(*this);
    GSBuggageLoader::InitDatums(*this);
    GSStaircase::InitDatums(*this);
    GSCatalog::GetInstance().LoadCatalog();

    m_ui.PostMessageUI(GSAliveUIAPI::SetGlobalStatus(true));
}

void GSSpawnerThread::OnDisconnect()
{ 
    OnSimStop(); 
    m_ui.PostMessageUI(GSAliveUIAPI::SetGlobalStatus(false));
}

void GSSpawnerThread::SetInjector(Injector injector, std::uint8_t groundServicedPercent)
{
    m_injector.store(injector, std::memory_order_relaxed);
    m_groundServicedPercent.store(groundServicedPercent, std::memory_order_relaxed);
    switch (injector) {
    case Injector::SayIntentions:
        GSLogStream::Log("Injector Selection: SayIntentions");
        break;
    case Injector::MSFS24Default:
        GSLogStream::Log("Injector Selection: MSFS24 Default, ") << static_cast<uint32_t>(groundServicedPercent) << "%";
        break;
    case Injector::Any:
        GSLogStream::Log("Injector Selection: Any, ") << static_cast<uint32_t>(groundServicedPercent) << "%";
        break;
    }
}

void GSSpawnerThread::OnCommand(GSCommand& cmd)
{
    switch (cmd.GetCmdID()) {
    case GS_CMD_SPAWNER_AIRCRAFT_ADDED:
        NewAircraft(static_cast<GSCmdAircraftAdd&>(cmd).GetAircraft());
        break;
    case GS_CMD_SPAWNER_AIRCRAFT_REMOVED:
        RemoveAircraft(static_cast<GSCmdAircraftUpdate&>(cmd).GetAircraftID());
        break;
    case GS_CMD_SPAWNER_AIRCRAFT_MODIFIED:
        ModAircraft(static_cast<GSCmdAircraftUpdate&>(cmd).GetAircraftID());
        break;
    case GS_CMD_AIRCRAFT_USER:
        UserAircraft(); // Implicit aquire fence 
        break;
    case GS_CMD_ANIM_OBJ_ADD:
        m_animThread.PostCommand(
            new GSCmdAnimObj(GS_CMD_ANIM_OBJ_ADD, *this, static_cast<GSCmdAnimObj&>(cmd).AnimObj().release()));
        break;
    case GS_CMD_ANIM_OBJ_REM:
        m_animThread.PostCommand(
            new GSCmdAnimObj(GS_CMD_ANIM_OBJ_REM, *this, static_cast<GSCmdAnimObj&>(cmd).ObjID(), static_cast<GSCmdAnimObj&>(cmd).ReleaseReq()));
        break;
    case GS_CMD_ANIM_OBJ_REM_DONE:
        static_cast<GSCmdAnimObj&>(cmd).GetReq().Process();
        break;
    case GS_CMD_MVMNT_OBJ_ADD:
        m_mvmntThread.PostCommand(
            new GSCmdSimObj(GS_CMD_MVMNT_OBJ_ADD, static_cast<GSCmdSimObj&>(cmd).SimObj()));
        break;
    case GS_CMD_MVMNT_OBJ_REM:
        m_mvmntThread.PostCommand(
            new GSCmdSimObj(GS_CMD_MVMNT_OBJ_REM, static_cast<GSCmdSimObj&>(cmd).SimObj()));
        break;
    case GS_CMD_MVMNT_OBJ_REM_RET:
        break;
    case GS_CMD_MVMNT_OBJ_ARR:
        static_cast<GSCmdSimObj&>(cmd).SimObj().OnArrived();
        break;
    default:
        GSLogStream::LogError("GSSpawnerThread - Unexpected cmd: ") << cmd.GetCmdID();
        break;
    }
}

void GSSpawnerThread::NewAircraft(std::shared_ptr<GSAircraft>& aircraft)
{
    if (m_groundPendingDelete.contains(aircraft->GetObjID())) {
        GSLogStream::Log("Frequent Respawn: ") << aircraft->GetObjID();
        if (!m_groundPendingAdd.emplace(aircraft->GetObjID(), aircraft).second) {
            GSLogStream::LogError("Aircraft already in PendingAdd: ") << aircraft->GetObjID();
        }
        return;
    }

    GSAircraftGround* grnd = nullptr;
    switch (aircraft->GetCategory()) {
    case GSAircraft::AircraftSizeCategory::Small:
        grnd = new GSAircraftGroundSmall(aircraft, *this, m_ui);
        break;
    case GSAircraft::AircraftSizeCategory::Large:
        grnd = new GSAircraftGroundLarge(aircraft, *this, m_ui);
        break;
    case GSAircraft::AircraftSizeCategory::ExtraLarge:
        grnd = new GSAircraftGroundXL(aircraft, *this, m_ui);
        break;
    default:
        grnd = new GSAircraftGroundMedium(aircraft, *this, m_ui);
        break;
    }

    auto itr = m_groundUnspawned.emplace(aircraft->GetObjID(), grnd);
    if (!itr.second) {
        GSLogStream::LogError("GSSpawnerThread - Existing aircraft as new");
        return;
    }

    m_ui.PostMessageUI(GSAliveUIAPI::AddAircraft(grnd->Aircraft()));
    CheckForUnspawned(*grnd);
}

void GSSpawnerThread::ModAircraft(DWORD aircraftID)
{
    auto itrPendAdd = m_groundPendingAdd.find(aircraftID);
    if (itrPendAdd != m_groundPendingAdd.end()) {
        return;
    }

    auto itr = m_groundUnspawned.find(aircraftID);
    if (itr != m_groundUnspawned.end()) {
        CheckForUnspawned(*itr->second);
    } else {
        auto itrSpw = m_groundSpawned.find(aircraftID);
        if (itrSpw != m_groundSpawned.end()) {
            CheckForSpawned(*(itrSpw->second));
        } else {
            GSLogStream::LogError("GSSpawnerThread::ModAircraft - Existing aircraft not found");
        }
    }
}

void GSSpawnerThread::RemoveAircraft(DWORD aircraftID)
{
    m_groundSpawnEligable.erase(aircraftID);
    m_groundPendingAdd.erase(aircraftID);
    m_groundPendingDelete.insert(m_groundUnspawned.extract(aircraftID));
    m_groundPendingDelete.insert(m_groundSpawned.extract(aircraftID));

    m_ui.PostMessageUI(GSAliveUIAPI::RemoveAircraft(aircraftID));
}

void GSSpawnerThread::UserAircraft()
{
    m_ui.PostMessageUI(GSAliveUIAPI::SetUserAircraft(m_userAircraft));

    HelperPrepare(m_groundUnspawned.size());

    std::for_each(m_groundUnspawned.begin(), m_groundUnspawned.end(), [this](auto& itr) {
        m_groundHelper.emplace_back(*(itr.second));
    });

    std::for_each(m_groundHelper.begin(), m_groundHelper.end(), [this](auto& grnd) {
        CheckForUnspawned(grnd);
    });
}

bool GSSpawnerThread::SpawnCond(const GSAircraft& aircraft)
{
    bool baseCond = (GSGeography::DistanceMeters(m_userAircraft.GetLongLat(), aircraft.GetLongLat()) < s_SpawnDistMeters) && (aircraft.GetGroundSpeedKnots() < 1.0);
    baseCond &= aircraft.IsOnGround();
    baseCond &= !aircraft.IsTaxing();
    return ((IsAnyInjector() && baseCond) ||
            (IsMSFSInjector() && baseCond && aircraft.HasParking()) ||
            (IsSIInjector() && baseCond && aircraft.IsLightNavOn()));
}

bool GSSpawnerThread::DespawnCond(const GSAircraft& aircraft)
{
    bool baseCond = (aircraft.GetGroundSpeedKnots() > 2.0) || aircraft.IsTaxing() || !aircraft.IsOnGround();
    return ((IsAnyInjector() && baseCond) ||
            (IsMSFSInjector() && baseCond) ||
            (IsSIInjector() && (baseCond || !aircraft.IsLightNavOn() || aircraft.IsLightBeaconOn())));
}

void GSSpawnerThread::CheckForUnspawned(GSAircraftGround& grnd)
{
    grnd.UpdateAircraft();
    if (grnd.GetSpawnState() == GSAircraftGround::SpawnState::UNSPAWNED) {
        if (SpawnCond(grnd.Aircraft())) {
            if (IsSIInjector() || (GSRandom::RandSizeT(1, 100) <= ServicedPerc())) {
                grnd.Spawn();
            } else {
                m_groundSpawnEligable.emplace(grnd.Aircraft().GetObjID());
            }

            m_groundSpawned.insert(m_groundUnspawned.extract(grnd.Aircraft().GetObjID()));
        }
    }
}

void GSSpawnerThread::CheckForSpawned(GSAircraftGround& grnd)
{
    grnd.UpdateAircraft();
    if (grnd.GetSpawnState() == GSAircraftGround::SpawnState::SPAWNED) {  
        if (DespawnCond(grnd.Aircraft())) {
            grnd.Despawn();
            TrySpawnEligable();
            m_groundUnspawned.insert(m_groundSpawned.extract(grnd.Aircraft().GetObjID()));
        }
    } 
}

void GSSpawnerThread::CheckForPendingRemove()
{
    HelperPrepare(m_groundPendingDelete.size());

    for (auto& itr : m_groundPendingDelete) {
        GSAircraftGround& grnd = *(itr.second);
        switch (grnd.GetSpawnState()) {
        case GSAircraftGround::SpawnState::SPAWNING:
            break;
        case GSAircraftGround::SpawnState::SPAWNED:
            grnd.Despawn();
            TrySpawnEligable();
            break;
        case GSAircraftGround::SpawnState::DESPAWNING:
            break;
        default:
            m_groundHelper.emplace_back(*itr.second);
            break;
        }
    }

    for (auto id : m_groundHelper) {
        m_groundPendingDelete.erase(id.get().Aircraft().GetObjID());
    }
}

void GSSpawnerThread::CheckForPendingAdd()
{
    std::list<DWORD> added;

    for (auto& itr : m_groundPendingAdd) {
        if (!m_groundPendingDelete.contains(itr.second->GetObjID())) {
            added.emplace_back(itr.second->GetObjID());
            NewAircraft(itr.second);
        }
    }

    for (auto val : added) {
        m_groundPendingAdd.erase(val);
    }
}

void GSSpawnerThread::HelperPrepare(size_t sz)
{
    m_groundHelper.reserve(sz);
    m_groundHelper.clear();
}

void GSSpawnerThread::TrySpawnEligable()
{
    while (!m_groundSpawnEligable.empty()) {
        const auto id = *m_groundSpawnEligable.begin();
        m_groundSpawnEligable.erase(id);

        const auto itr = m_groundSpawned.find(id);
        if (itr == m_groundSpawned.end()) {
            GSLogStream::LogError("GSSpawnerThread::TrySpawnEligable - Eligible aircraft not found: ") << id;
            continue;
        }

        GSAircraftGround& grnd = *itr->second;
        if (grnd.GetSpawnState() != GSAircraftGround::SpawnState::UNSPAWNED) {
            GSLogStream::LogError("GSSpawnerThread::TrySpawnEligable - Eligible aircraft is not unspawned: ") << id;
            continue;
        }

        grnd.Spawn();
        return;
    }
}

} // namespace NS_GSAliveAirport
