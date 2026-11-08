// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimConnect.h"
#include "SimObjects/GSUtilityGround.h"
#include <list>
#include <memory>
#include <thread>

namespace NS_GSAliveAirport
{

class GSAliveUI;
class GSAirport;

class GSUtilityGroundThread final : public GSSimConnect
{
public:
    GSUtilityGroundThread(GSAliveUI& ui, GSSimConnect& mvmntThread):
        m_ui(ui), m_mvmntThread(mvmntThread) {}
    ~GSUtilityGroundThread() override { Stop(); }
    GSUtilityGroundThread(const GSUtilityGroundThread&) = delete;
    GSUtilityGroundThread& operator=(const GSUtilityGroundThread&) = delete;

    void Start();
    void Stop();

    void OnConnect() override {}
    void OnDisconnect() override { OnSimStop(); }
    void OnSimStart() override {}
    void OnSimStop() override;

private:
    const char* GetDebugName() const override { return "GSUtilityGroundThread"; }
    void OnCommand(GSCommand& cmd) override;
    void OnUserAirport(std::shared_ptr<GSAirport>& airport);
    void RemoveActiveAirport();
    void CheckForPendingRemove();

    GSAliveUI& m_ui;
    GSSimConnect& m_mvmntThread;
    std::unique_ptr<GSUtilityGround> m_activeAirport;
    std::list<std::unique_ptr<GSUtilityGround>> m_pendingRemoved;
    std::jthread m_thread;
};

} // namespace NS_GSAliveAirport
