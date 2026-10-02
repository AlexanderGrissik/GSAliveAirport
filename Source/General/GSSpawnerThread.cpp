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
#include "../Commands/GSCmdAircraftUpdate.h"
#include "../Commands/GSCmdAnimObj.h"
#include "../Commands/GSCmdSimObj.h"
#include "../Commands/GSCmdAirport.h"
#include "GSCatalog.h"
#include "GSGeography.h"
#include <chrono>
#include <array>
#include <algorithm>

using namespace std::chrono_literals;

namespace NS_GSAliveAirport
{

const double GSSpawnerThread::s_SpawnDistMeters = 1000.0;

void GSSpawnerThread::Start()
{
    if (m_thread.joinable()) return;
    m_thread = std::jthread([this](std::stop_token stopToken) {
        while (!stopToken.stop_requested()) {
            RunDispatch(stopToken);
            CheckForPendingUpdate();
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
    GSCatalog::GetInstance().LoadCatalog();
}

void GSSpawnerThread::OnCommand(GSCommand& cmd)
{
    switch (cmd.GetCmdID()) {
    case GSDefinitions::CMD_SPAWNER_AIRCRAFT_ADDED:
        NewAircraft(*static_cast<GSCmdAircraftUpdate&>(cmd).GetAircraft());
        break;
    case GSDefinitions::CMD_SPAWNER_AIRCRAFT_REMOVED:
        RemoveAircraft(*static_cast<GSCmdAircraftUpdate&>(cmd).GetAircraft());
        break;
    case GSDefinitions::CMD_SPAWNER_AIRCRAFT_MODIFIED:
        ModAircraft(*static_cast<GSCmdAircraftUpdate&>(cmd).GetAircraft());
        break;
    case GSDefinitions::CMD_SPAWNER_AIRCRAFT_USER:
        UserAircraft(*static_cast<GSCmdAircraftUpdate&>(cmd).GetAircraft());
        break;
    case GSDefinitions::CMD_ANIM_OBJ_ADD: {
        auto& cmdAnim = static_cast<GSCmdAnimObj&>(cmd);
        CmdPtr newCmd(new GSCmdAnimObj(GSDefinitions::CMD_ANIM_OBJ_ADD, *this, cmdAnim.AnimObj().release()));
        m_animThread.PostCommand(newCmd);
        break;
    }
    case GSDefinitions::CMD_ANIM_OBJ_REM: {
        auto& cmdAnim = static_cast<GSCmdAnimObj&>(cmd);
        CmdPtr newCmd(new GSCmdAnimObj(GSDefinitions::CMD_ANIM_OBJ_REM, *this, cmdAnim.ObjID(), cmdAnim.ReleaseReq()));
        m_animThread.PostCommand(newCmd);
        break;
    }
    case GSDefinitions::CMD_ANIM_OBJ_REM_DONE: {
        auto& cmdAnim = static_cast<GSCmdAnimObj&>(cmd);
        cmdAnim.GetReq().Process();
        break;
    }
    case GSDefinitions::CMD_MVMNT_OBJ_ADD: {
        auto& cmdObj = static_cast<GSCmdSimObj&>(cmd);
        CmdPtr newCmd(new GSCmdSimObj(GSDefinitions::CMD_MVMNT_OBJ_ADD, cmdObj.SimObj()));
        m_mvmntThread.PostCommand(newCmd);
        break;
    }
    case GSDefinitions::CMD_MVMNT_OBJ_REM: {
        auto& cmdObj = static_cast<GSCmdSimObj&>(cmd);
        CmdPtr newCmd(new GSCmdSimObj(GSDefinitions::CMD_MVMNT_OBJ_REM, cmdObj.SimObj()));
        m_mvmntThread.PostCommand(newCmd);
        break;
    }
    case GSDefinitions::CMD_MVMNT_OBJ_REM_RET: {
        break;
    }
    case GSDefinitions::CMD_MVMNT_OBJ_ARR: {
        auto& cmdObj = static_cast<GSCmdSimObj&>(cmd);
        cmdObj.SimObj().OnArrived();
        break;
    }
    case GSDefinitions::CMD_AIRPORT: {
        auto& cmdObj = static_cast<GSCmdAirport&>(cmd);
        m_airport = cmdObj.Airport();
        GSLogStream::Log("Current Airport: ") << m_airport->GetICAO().c_str();
        break;
    }
    default:
        GSLogStream::LogError("GSSpawnerThread - Unexpected cmd: ") << cmd.GetCmdID();
        break;
    }
}

void GSSpawnerThread::NewAircraft(const GSAircraft& aircraft)
{
    if (m_groundPendingDelete.contains(aircraft.GetObjID())) {
        GSLogStream::Log("Frequent Respawn: ") << aircraft.GetObjID();
        if (!m_groundPendingAdd.emplace(aircraft.GetObjID(), aircraft).second) {
            GSLogStream::LogError("Aircraft already in PendingAdd: ") << aircraft.GetObjID();
        }
        return;
    }

    GSAircraftGround* grnd = nullptr;
    switch (aircraft.GetCategory()) {
    case GSAircraft::AircraftSizeCategory::Small:
        grnd = new GSAircraftGroundSmall(aircraft, *this, m_airport);
        break;
    case GSAircraft::AircraftSizeCategory::Large:
        grnd = new GSAircraftGroundLarge(aircraft, *this, m_airport);
        break;
    case GSAircraft::AircraftSizeCategory::ExtraLarge:
        grnd = new GSAircraftGroundXL(aircraft, *this, m_airport);
        break;
    default:
        grnd = new GSAircraftGroundMedium(aircraft, *this, m_airport);
        break;
    }

    auto itr = m_groundUnspawned.emplace(aircraft.GetObjID(), grnd);
    if (!itr.second) {
        GSLogStream::LogError("GSSpawnerThread - Existing aircraft as new");
        return;
    }

    CheckForUnspawned(*itr.first->second, aircraft);
}

void GSSpawnerThread::ModAircraft(const GSAircraft& aircraft)
{
    auto itrPendAdd = m_groundPendingAdd.find(aircraft.GetObjID());
    if (itrPendAdd != m_groundPendingAdd.end()) {
        itrPendAdd->second.CopyDynInfo(aircraft);
        return;
    }

    auto itr = m_groundUnspawned.find(aircraft.GetObjID());
    if (itr != m_groundUnspawned.end()) {
        CheckForUnspawned(*itr->second, aircraft);
    } else {
        auto itrSpw = m_groundSpawned.find(aircraft.GetObjID());
        if (itrSpw != m_groundSpawned.end()) {
            CheckForSpawned(*(itrSpw->second), aircraft);
        } else {
            GSLogStream::LogError("GSSpawnerThread::ModAircraft - Existing aircraft not found");
        }
    }
}

void GSSpawnerThread::RemoveAircraft(const GSAircraft& aircraft)
{
    auto objID = aircraft.GetObjID();
    m_groundPendingUpdate.erase(objID);
    m_groundPendingAdd.erase(objID);
    m_groundPendingDelete.insert(m_groundUnspawned.extract(objID));
    m_groundPendingDelete.insert(m_groundSpawned.extract(objID));
}

void GSSpawnerThread::UserAircraft(const GSAircraft& aircraft)
{
    m_userPos = aircraft.GetLongLat();

    HelperPrepare(m_groundUnspawned.size());

    std::for_each(m_groundUnspawned.begin(), m_groundUnspawned.end(), [this](auto& itr) {
        m_groundHelper.emplace_back(*(itr.second));
    });

    std::for_each(m_groundHelper.begin(), m_groundHelper.end(), [this](auto& grnd) {
        CheckForUnspawned(grnd, grnd.get().Aircraft());
    });
}

bool GSSpawnerThread::SpawnCond(const GSAircraft& aircraft)
{
    const auto& aircraftData = aircraft.GetRawData();
    bool baseCond = (GSGeography::DistanceMeters(m_userPos, aircraft.GetLongLat()) < s_SpawnDistMeters) && (aircraftData.groundSpeedKnots < 1.0);
    bool defCond = aircraft.HasParking();
    return (baseCond && (defCond || (aircraftData.lightNav && !aircraft.IsTaxing())));
}

bool GSSpawnerThread::DespawnCond(const GSAircraft& aircraft)
{
    const auto& aircraftData = aircraft.GetRawData();
    return ((!aircraftData.lightNav && !aircraft.HasParking()) || (aircraftData.groundSpeedKnots > 2.0) || aircraft.IsTaxing());
}

void GSSpawnerThread::CheckForUnspawned(GSAircraftGround& grnd, const GSAircraft& aircraftUpdated)
{
    if (grnd.GetSpawnState() == GSAircraftGround::SpawnState::UNSPAWNED) {
        grnd.UpdateAircraft(aircraftUpdated);
        if (SpawnCond(aircraftUpdated)) {
            grnd.Spawn();
            m_groundSpawned.insert(m_groundUnspawned.extract(aircraftUpdated.GetObjID()));
        }
    } else { // DESPAWNING
        AddPendingUpdate(aircraftUpdated);
    }
}

void GSSpawnerThread::CheckForSpawned(GSAircraftGround& grnd, const GSAircraft& aircraftUpdated)
{
    if (grnd.GetSpawnState() == GSAircraftGround::SpawnState::SPAWNED) {
        grnd.UpdateAircraft(aircraftUpdated);
        if (DespawnCond(aircraftUpdated)) {
            grnd.Despawn();
            m_groundUnspawned.insert(m_groundSpawned.extract(aircraftUpdated.GetObjID()));
        }
    } else { // SPWANING
        AddPendingUpdate(aircraftUpdated);
    }
}

void GSSpawnerThread::AddPendingUpdate(const GSAircraft& aircraftUpdated)
{
    m_groundPendingUpdate.insert_or_assign(aircraftUpdated.GetObjID(), aircraftUpdated);
}

void GSSpawnerThread::CheckForPendingUpdate()
{
    decltype(m_groundPendingUpdate) temp;
    temp.swap(m_groundPendingUpdate);

    for (auto& itr : temp) {
        ModAircraft(itr.second);
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
        if (!m_groundPendingDelete.contains(itr.second.GetObjID())) {
            added.emplace_back(itr.second.GetObjID());
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

} // namespace NS_GSAliveAirport
