// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimConnect.h"
#include "../SimObjects/GSAircraftGround.h"
#include "../UI/GSAliveUI.h"
#include "GSCoord.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace NS_GSAliveAirport
{

class GSSpawnerThread final : public GSSimConnect
{
public:

    enum class Injector : std::uint8_t
    {
        SayIntentions = 0,
        MSFS24Default = 1,
        Any = 99,
    };

    static constexpr double s_SpawnDistMeters = 1000.0;

    GSSpawnerThread(GSSimConnect& animThread, GSSimConnect& mvmntThread, GSAliveUI& ui, GSAircraft& userAircraft):
        m_animThread(animThread), m_mvmntThread(mvmntThread), m_ui(ui), m_userAircraft(userAircraft) {}
    ~GSSpawnerThread() override {}
    GSSpawnerThread(const GSSpawnerThread &) = delete;
    GSSpawnerThread &operator=(const GSSpawnerThread &) = delete;

    void Start();
    void Stop();

    void OnConnect() override;
    void OnDisconnect() override;
    void OnSimStart() override {}
    void OnSimStop() override {}
    void OnCommand(GSCommand& cmd) override;

    void SetInjector(Injector injector, std::uint8_t groundServicedPercent);
    bool IsAnyInjector() const { return m_injector.load(std::memory_order_relaxed) == Injector::Any; }
    bool IsSIInjector() const { return m_injector.load(std::memory_order_relaxed) == Injector::SayIntentions; }
    bool IsMSFSInjector() const { return m_injector.load(std::memory_order_relaxed) == Injector::MSFS24Default; }
    uint8_t ServicedPerc() const { return m_groundServicedPercent.load(std::memory_order_relaxed); }

private:

    const char* GetDebugName() const override { return "GSSpawnerThread"; }

    void HelperPrepare(size_t sz);
    void TrySpawnEligable();
    void NewAircraft(std::shared_ptr<GSAircraft>& aircraft);
    void ModAircraft(DWORD aircraftID);
    void RemoveAircraft(DWORD aircraftID);
    void UserAircraft();
    bool SpawnCond(const GSAircraft& aircraft);
    bool DespawnCond(const GSAircraft& aircraft);
    void CheckForUnspawned(GSAircraftGround& grnd);
    void CheckForSpawned(GSAircraftGround& grnd);
    void CheckForPendingRemove();
    void CheckForPendingAdd();

    std::jthread m_thread;
    std::atomic<Injector> m_injector {Injector::Any};
    std::atomic<std::uint8_t> m_groundServicedPercent {50};

    using GroundMap = std::unordered_map<DWORD, std::unique_ptr<GSAircraftGround>>;
    GroundMap m_groundUnspawned;
    GroundMap m_groundSpawned;
    GroundMap m_groundPendingDelete;
    std::unordered_map<DWORD, std::shared_ptr<GSAircraft>> m_groundPendingAdd;
    std::vector<std::reference_wrapper<GSAircraftGround>> m_groundHelper;
    std::unordered_set<DWORD> m_groundSpawnEligable;
    GSSimConnect& m_animThread;
    GSSimConnect& m_mvmntThread;
    GSAliveUI& m_ui;
    GSAircraft& m_userAircraft;
};
} // namespace NS_GSAliveAirport
