#pragma once

#include "GSSimConnect.h"

#include <stop_token>
#include <thread>

namespace NS_GSLiveAirportMSFS
{

class GSSpawnerThread final : public GSSimConnect
{
public:

    enum ECommands
    {
        CMD_SPAWNER_AIRCRAFT_ADDED,
        CMD_SPAWNER_AIRCRAFT_REMOVED,
        CMD_SPAWNER_AIRCRAFT_MODIFIED
    };

    GSSpawnerThread() = default;
    ~GSSpawnerThread() override {}
    GSSpawnerThread(const GSSpawnerThread &) = delete;
    GSSpawnerThread &operator=(const GSSpawnerThread &) = delete;

    void Start();
    void Stop();

    void OnConnect() override {}
    void OnDisconnect() override {}
    void OnSimStart() override { m_simStarted = true; }
    void OnSimStop() override { m_simStarted = false; }
    void OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override;
    void OnException(SIMCONNECT_RECV_EXCEPTION *message) override;
    bool OnCommand(GSCommand& cmd) override;

private:
    bool m_simStarted = false;
    std::jthread m_thread;
};
} // namespace NS_GSLiveAirportMSFS