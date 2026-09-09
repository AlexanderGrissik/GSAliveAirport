#include "GSSimConnect.h"
#include "GSLogStream.h"
#include "GSDefinitions.h"

#include <chrono>
#include <cstring>
#include <thread>
using namespace std::chrono_literals;

namespace NS_GSLiveAirportMSFS
{

void GSSimConnect::RunDispatch(std::stop_token stopToken)
{
    m_lastDispatch = true;
    while (!stopToken.stop_requested() && m_lastDispatch) {
        if (!m_handle) {
            Connect(stopToken);
        }

        m_lastDispatch = false;
        const HRESULT dispatch = SimConnect_CallDispatch(m_handle, DispatchThunk, this);
        if (m_disconnectRequested || FAILED(dispatch)) {
            if (FAILED(dispatch) && !m_disconnectRequested) {
                GSLogStream::Log("SimConnect dispatch failed; reconnecting.");
            }
            Disconnect();
            m_disconnectRequested = false;
            break;
        }
    }
}

void GSSimConnect::Connect(std::stop_token stopToken)
{
    bool reportedWaiting = false;
    while (!stopToken.stop_requested() && !m_handle) {
        if (FAILED(SimConnect_Open(&m_handle, "GSLiveAirportMSFS", nullptr, 0, nullptr, 0))) {
            m_handle = nullptr;
        }

        if (m_handle && FAILED(SimConnect_SubscribeToSystemEvent(m_handle, GSDefinitions::GSEventID_SimState, "Sim"))) {
            GSLogStream::Log(std::string("Failed to subscribe to SimConnect event: Sim"));
            SimConnect_Close(m_handle);
            m_handle = nullptr;
            return;
        }

        if (!m_handle) {
            if (!reportedWaiting) {
                GSLogStream::Log("Waiting for Microsoft Flight Simulator 2024...");
                reportedWaiting = true;
            }
            std::this_thread::sleep_for(2000ms);
        }
    }

    GSLogStream::Log("Connected to MSFS 2024.");
    OnConnect();
}

void GSSimConnect::Disconnect()
{
    if (m_handle) {
        SimConnect_Close(m_handle);
        m_handle = nullptr;
    }
}

void GSSimConnect::OnSimConnectMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    switch (message->dwID) {
    case SIMCONNECT_RECV_ID_QUIT:
        GSLogStream::Log("MSFS24 closed the SimConnect connection.");
        m_disconnectRequested = true;
        OnDisconnect();
        break;
    case SIMCONNECT_RECV_ID_EVENT: {
        const auto &event = *reinterpret_cast<SIMCONNECT_RECV_EVENT *>(message);
        if (event.uEventID == GSDefinitions::GSEventID_SimState) {
            if (event.dwData != 0) {
                GSLogStream::Log("Simulation started.");
                OnSimStart();
            } else {
                GSLogStream::Log("Simulation stopped.");
                OnSimStop();
            }
        } else {
            OnMessage(message, messageSize);
        }
        break;
    }
    case SIMCONNECT_RECV_ID_EXCEPTION:
        OnException(reinterpret_cast<SIMCONNECT_RECV_EXCEPTION *>(message));
        break;
    default:
        OnMessage(message, messageSize);
        break;
    }

    m_lastDispatch = true;
}

void CALLBACK GSSimConnect::DispatchThunk(SIMCONNECT_RECV *message, DWORD messageSize, void *context)
{
    reinterpret_cast<GSSimConnect *>(context)->OnSimConnectMessage(message, messageSize);
}

bool GSSimConnect::InvokeAddDatum(SIMCONNECT_DATA_DEFINITION_ID DefineID, const char * DatumName, const char * UnitsName, SIMCONNECT_DATATYPE DatumType)
{
    return CaptureResult(
        SimConnect_AddToDataDefinition(m_handle, DefineID, DatumName, UnitsName, DatumType), 0).isOK();
}

void GSSimConnect::RunCommands()
{
    bool cont = false;
    do {
        
        auto opt = m_commands.TryPop();
        if (opt.has_value()) {
            cont = OnCommand(*opt->get());
        }
    } while (cont);
}

GSSimConnect::SendResult GSSimConnect::CaptureResult(HRESULT result, DWORD requestID) const
{
    DWORD sendId = 0;
    if (GetHandle() && SUCCEEDED(result)) {
        if (!SUCCEEDED(SimConnect_GetLastSentPacketID(GetHandle(), &sendId))) {
            sendId = 0;
        }
    }
    return {result, sendId, requestID};
}

void GSSimConnect::ReadMsgData(void* dest, size_t destSize, const SIMCONNECT_RECV_SIMOBJECT_DATA& entry)
{
    const auto* begin = reinterpret_cast<const BYTE*>(&entry);
    const auto* data = reinterpret_cast<const BYTE*>(&entry.dwData);
    const size_t offset = data - begin;
    const size_t payloadSize = entry.dwSize - offset;
    std::memcpy(dest, data, std::min(payloadSize, destSize));
}

} // namespace NS_GSLiveAirportMSFS
