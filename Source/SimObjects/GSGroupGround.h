// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "General/GSDefinitions.h"
#include "GSSimObj.h"
#include <cstddef>
#include <list>
#include <memory>

namespace NS_GSAliveAirport
{

class GSAliveUI;

class GSGroupGround : public GSSimObjUpdate
{
public:

    GSGroupGround(GSSimConnect& simConn, GSAliveUI& ui):
        m_simConnect(simConn), m_ui(ui) {}
    ~GSGroupGround() override = default;

    void OnSpawned(bool ok, GSSimObj& obj) override;
    void OnDespawned() override;
    void Spawn();
    void Despawn();

    SpawnState GetSpawnState() const { return m_spawnState; }

protected:

    virtual void BuildObjs() = 0;
    virtual void Spawning() = 0;
    virtual void Spawned() = 0;
    virtual void Despawning() = 0;
    virtual void Unspawned() = 0;

    GSSimConnect& m_simConnect;
    SpawnState m_spawnState = SpawnState::UNSPAWNED;
    std::list<std::unique_ptr<GSSimObj>> m_objs;
    size_t m_spawnedObjs = 0U;
    GSAliveUI& m_ui;
};

} // namespace NS_GSAliveAirport
