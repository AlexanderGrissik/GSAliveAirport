// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSUtilityGroundThread.h"
#include "Commands/GSCmdAirport.h"
#include "GSLogStream.h"
#include <chrono>
#include <stop_token>
#include <utility>

namespace NS_GSAliveAirport
{
using namespace std::chrono_literals;

void GSUtilityGroundThread::Start()
{
    if (m_thread.joinable()) return;
    m_thread = std::jthread([this](std::stop_token stopToken) {
        while (!stopToken.stop_requested()) {
            RunDispatch(stopToken);
            CheckForPendingRemove();
            std::this_thread::sleep_for(IsSimStarted() && IsLastLoopMessage() ? 50ms : 1000ms);
        }
        OnDisconnect();
        Disconnect();
    });
}

void GSUtilityGroundThread::Stop()
{
    if (!m_thread.joinable()) return;
    m_thread.request_stop();
    m_thread.join();
}

void GSUtilityGroundThread::OnSimStop()
{
    RemoveActiveAirport();
}

void GSUtilityGroundThread::OnCommand(GSCommand& cmd)
{
    switch (cmd.GetCmdID()) {
    case GS_CMD_AIRPORT_USER:
        OnUserAirport(static_cast<GSCmdAirport&>(cmd).GetAirport());
        break;
    default:
        GSLogStream::LogError("GSUtilityGroundThread - Unexpected cmd: ") << cmd.GetCmdID();
        break;
    }
}

void GSUtilityGroundThread::OnUserAirport(std::shared_ptr<GSAirport>& airport)
{
    if (m_activeAirport && m_activeAirport->GetAirport()->GetICAO() == airport.get()->GetICAO()) {
        return;
    }

    RemoveActiveAirport();

    m_activeAirport = std::make_unique<GSUtilityGround>(airport, *this, m_ui);
    m_activeAirport->Spawn();
}

void GSUtilityGroundThread::RemoveActiveAirport()
{
    if (m_activeAirport) {
        m_pendingRemoved.emplace_back(std::move(m_activeAirport));
    }
}

void GSUtilityGroundThread::CheckForPendingRemove()
{
    std::erase_if(m_pendingRemoved, [](const auto& ground) {
        if (ground->GetSpawnState() == SpawnState::SPAWNED) {
            ground->Despawn();
        }
        return ground->GetSpawnState() == SpawnState::UNSPAWNED;
    });
}

} // namespace NS_GSAliveAirport
