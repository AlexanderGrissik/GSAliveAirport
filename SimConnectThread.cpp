#include "SimConnectThread.h"

namespace parking_services
{

void SimConnectThread::RunDispatch(std::stop_token stopToken)
{
    while (!stopToken.stop_requested() && !m_handle) {
        if (!m_handle) {
            Connect();
        }
         
        const HRESULT dispatch = SimConnect_CallDispatch(m_handle, DispatchThunk, this);
        if (m_disconnectRequested || FAILED(dispatch)) {
            if (FAILED(dispatch) && !m_disconnectRequested) {
                GSLog("SimConnect dispatch failed; reconnecting.");
            }
            Disconnect();
            m_disconnectRequested = false;
        }
    }
}

void SimConnectThread::Connect()
{
    bool reportedWaiting = false;
    while (!stopToken.stop_requested() && !m_handle) {
        if (FAILED(SimConnect_Open(&m_handle, "ParkingServices", nullptr, 0, nullptr, 0))) {
            m_handle = nullptr;
        }

        if (m_handle && FAILED(SimConnect_SubscribeToSystemEvent(m_handle, GSEventID_SimState, "Sim"))) {
            GSLog(std::string("Failed to subscribe to SimConnect event: Sim"));
            SimConnect_Close(m_handle);
            m_handle = nullptr;
            return false;
        }

        if (!m_handle) {
            if (!reportedWaiting) {
                GSLog("Waiting for Microsoft Flight Simulator 2024...");
                reportedWaiting = true;
            }
            std::this_thread::sleep_for(2000ms);
        }
    }

    GSLog("Connected to MSFS 2024.");
    OnConnect();
}

void SimConnectThread::Disconnect()
{
    if (m_handle) {
        SimConnect_Close(m_handle);
        m_handle = nullptr;
    }
}

void SimConnectThread::OnSimConnectMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    switch (message->dwID) {
    case SIMCONNECT_RECV_ID_QUIT:
        GSLog("MSFS24 closed the SimConnect connection.");
        m_disconnectRequested = true;
        OnDisconnect();
        break;
    case SIMCONNECT_RECV_ID_EVENT: {
        const auto &event = *reinterpret_cast<SIMCONNECT_RECV_EVENT *>(message);
        if (event.uEventID == GSEventID_SimState) {
            if (event.dwData != 0) {
                GSLog("Simulation started.");
                OnSimStart();
            } else {
                GSLog("Simulation stopped.");
                OnSimEnd();
            }
        } else {
            OnMessage(*message, messageSize);
        }
        break;
    case SIMCONNECT_RECV_ID_EXCEPTION:
        OnException(*reinterpret_cast<SIMCONNECT_RECV_EXCEPTION *>(message))
        break;
    default:
        OnMessage(*message, messageSize);
        break;
    }
}

void CALLBACK SimConnectThread::DispatchThunk(SIMCONNECT_RECV *message, DWORD messageSize, void *context)
{
    reinterpret_cast<SimConnectThread *>(context)->OnSimConnectMessage(message, messageSize);
}

bool SimConnectThread::InvokeAddDatum(SIMCONNECT_DATA_DEFINITION_ID DefineID, const char * DatumName, const char * UnitsName, SIMCONNECT_DATATYPE DatumType)
{
    return CaptureResult(
        SimConnect_AddToDataDefinition(m_handle, DefineID, DatumName, UnitsName, DatumType), 0).ok;
}

} // namespace parking_services
