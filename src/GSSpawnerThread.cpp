#include "GSSpawnerThread.h"
#include "GSLogStream.h"
#include "simobj/GSAircraftGroundSmall.h"
#include "simobj/GSAircraftGroundMedium.h"
#include "simobj/GSAircraftGroundLarge.h"
#include "simobj/GSAircraftGroundXL.h"
#include "simobj/GSCateringCartMSFS.h"
#include "cmds/GSCmdAircraftUpdate.h"
#include "GSGeography.h"
#include <chrono>
#include <array>
#include <algorithm>

using namespace std::chrono_literals;

namespace NS_GSLiveAirportMSFS
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
    GSCateringCartMSFS::InitDatums(*this); 
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
    default:
        GSLogStream::LogError("GSSpawnerThread - Unexpected cmd: ") << cmd.GetCmdID();
        break;
    }
}

void GSSpawnerThread::NewAircraft(const GSAircraft& aircraft)
{
    GSAircraftGround* grnd = nullptr;
    switch (aircraft.GetCategory()) {
    case GSAircraft::AircraftSizeCategory::Small:
        grnd = new GSAircraftGroundSmall(aircraft, *this);
        break;
    case GSAircraft::AircraftSizeCategory::Large:
        grnd = new GSAircraftGroundLarge(aircraft, *this);
        break;
    case GSAircraft::AircraftSizeCategory::ExtraLarge:
        grnd = new GSAircraftGroundXL(aircraft, *this);
        break;
    default:
        grnd = new GSAircraftGroundMedium(aircraft, *this);
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
    m_groundPendingDelete.insert(m_groundUnspawned.extract(objID));
    m_groundPendingDelete.insert(m_groundSpawned.extract(objID));
    GSLogStream::LogError("GSSpawnerThread::RemoveAircraft - AircraftGround Removed: ") << objID;
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
    return ((GSGeography::DistanceMeters(m_userPos, aircraft.GetLongLat()) < s_SpawnDistMeters) &&
            aircraftData.lightNav && (aircraftData.groundSpeedKnots < 1.0) && !aircraft.IsTaxing());
}

bool GSSpawnerThread::DespawnCond(const GSAircraft& aircraft)
{
    const auto& aircraftData = aircraft.GetRawData();
    return (!aircraftData.lightNav || (aircraftData.groundSpeedKnots > 2.0) || aircraft.IsTaxing());
}

void GSSpawnerThread::CheckForUnspawned(GSAircraftGround& grnd, const GSAircraft& aircraftUpdated)
{
    if (grnd.GetSpawnState() == GSAircraftGround::SpawnState::UNSPAWNED) {
        grnd.UpdateAircraft(aircraftUpdated);
        if (SpawnCond(aircraftUpdated)) {
            grnd.Spawn();
            GSLogStream::Log("GSSpawnerThread - New AircraftGround: ") << aircraftUpdated.GetObjID();
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

void GSSpawnerThread::HelperPrepare(size_t sz)
{
    m_groundHelper.reserve(sz);
    m_groundHelper.clear();
}

} // namespace NS_GSLiveAirportMSFS
