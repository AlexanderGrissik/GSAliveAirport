#pragma once

#include "GSSimConnect.h"
#include "../SimObjects/GSAircraftGround.h"
#include "GSCoord.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <vector>

namespace NS_GSLiveAirportMSFS
{

class GSSpawnerThread final : public GSSimConnect
{
public:

    static const double s_SpawnDistMeters;

    GSSpawnerThread(GSSimConnect& animThread, GSSimConnect& mvmntThread):
        m_animThread(animThread), m_mvmntThread(mvmntThread) {}
    ~GSSpawnerThread() override {}
    GSSpawnerThread(const GSSpawnerThread &) = delete;
    GSSpawnerThread &operator=(const GSSpawnerThread &) = delete;

    void Start();
    void Stop();

    void OnConnect() override;
    void OnDisconnect() override { OnSimStop(); }
    void OnSimStart() override {}
    void OnSimStop() override {}
    void OnCommand(GSCommand& cmd) override;

private:

    void HelperPrepare(size_t sz);

    void NewAircraft(const GSAircraft& aircraft);
    void ModAircraft(const GSAircraft& aircraft);
    void RemoveAircraft(const GSAircraft& aircraft);
    void UserAircraft(const GSAircraft& aircraft);
    bool SpawnCond(const GSAircraft& aircraft);
    bool DespawnCond(const GSAircraft& aircraft);
    void CheckForUnspawned(GSAircraftGround& grnd, const GSAircraft& aircraftUpdated);
    void CheckForSpawned(GSAircraftGround& grnd, const GSAircraft& aircraftUpdated);
    void AddPendingUpdate(const GSAircraft& aircraftUpdated);
    void CheckForPendingUpdate();
    void CheckForPendingRemove();

    std::jthread m_thread;
    GSCoord m_userPos;

    using GroundMap = std::unordered_map<DWORD, std::unique_ptr<GSAircraftGround>>;
    GroundMap m_groundUnspawned;
    GroundMap m_groundSpawned;
    GroundMap m_groundPendingDelete;
    std::unordered_map<DWORD, GSAircraft> m_groundPendingUpdate;
    std::vector<std::reference_wrapper<GSAircraftGround>> m_groundHelper;
    std::shared_ptr<GSAirport> m_airport;
    GSSimConnect& m_animThread;
    GSSimConnect& m_mvmntThread;
};
} // namespace NS_GSLiveAirportMSFS
