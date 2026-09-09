#include "GSSpawnerThread.h"

#include "GSLogStream.h"

#include <chrono>
using namespace std::chrono_literals;

namespace NS_GSLiveAirportMSFS
{

void GSSpawnerThread::Start()
{
    if (m_thread.joinable()) return;
    m_thread = std::jthread([this](std::stop_token stopToken) {
        while (!stopToken.stop_requested()) {
            RunDispatch(stopToken);
            RunCommands();
            if (!m_simStarted) {
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

void GSSpawnerThread::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    GSLogStream::LogError("GSSpawnerThread - Unexpected message: ") << message->dwID << ", Size: " << messageSize << std::endl;
}

void GSSpawnerThread::OnException(SIMCONNECT_RECV_EXCEPTION *message)
{
    GSLogStream::LogError("GSSpawnerThread - Exception: ") << message->dwID << ", Exception: " << message->dwException << " Index:" << message->dwIndex << std::endl;
}

bool GSSpawnerThread::OnCommand(GSCommand& cmd)
{
    switch (cmd.GetCmdID()) {
    case CMD_SPAWNER_AIRCRAFT_ADDED:
        GSLogStream::LogError("GSAircraftTrackerThread1") << cmd.GetCmdID() << std::endl;
        break;
    case CMD_SPAWNER_AIRCRAFT_REMOVED:
        GSLogStream::LogError("GSAircraftTrackerThread2") << cmd.GetCmdID() << std::endl;
        break;
    case CMD_SPAWNER_AIRCRAFT_MODIFIED:
        GSLogStream::LogError("GSAircraftTrackerThread3") << cmd.GetCmdID() << std::endl;
        break;
    default:
        GSLogStream::LogError("GSAircraftTrackerThread - Unexpected cmd: ") << cmd.GetCmdID() << std::endl;
        break;
    }

    return true;
}

} // namespace NS_GSLiveAirportMSFS