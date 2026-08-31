#include "SimConnectSession.h"

#include <algorithm>
#include <utility>

namespace parking_services
{
SimConnectSession::SimConnectSession(LogSink log) : m_log(std::move(log)) {}

SimConnectSession::~SimConnectSession()
{
    Close();
}

bool SimConnectSession::Open(ISimConnectMessageSink &sink)
{
    if (m_handle) {
        return true;
    }

    HANDLE handle = nullptr;
    if (FAILED(SimConnect_Open(&handle, "ParkingServices", nullptr, 0, nullptr, 0))) {
        return false;
    }
    m_handle = handle;
    m_sink = &sink;
    m_connected.store(true);
    return true;
}

void SimConnectSession::Close()
{
    m_connected.store(false);
    m_sink = nullptr;
    if (m_handle) {
        SimConnect_Close(m_handle);
        m_handle = nullptr;
    }
    m_pendingOperations.clear();
}

bool SimConnectSession::IsConnected() const
{
    return m_connected.load();
}

HRESULT SimConnectSession::Dispatch()
{
    return m_handle ? SimConnect_CallDispatch(m_handle, DispatchThunk, this) : E_HANDLE;
}

bool SimConnectSession::AddDatum(SIMCONNECT_DATA_DEFINITION_ID definition, const char *name,
                                 const char *units, SIMCONNECT_DATATYPE type)
{
    if (!m_handle || FAILED(SimConnect_AddToDataDefinition(m_handle, definition, name, units, type))) {
        m_log(std::string("Failed to define SimVar: ") + name);
        return false;
    }
    return true;
}

bool SimConnectSession::SubscribeSystemEvent(SIMCONNECT_CLIENT_EVENT_ID eventId, const char *name)
{
    if (!m_handle || FAILED(SimConnect_SubscribeToSystemEvent(m_handle, eventId, name))) {
        m_log(std::string("Failed to subscribe to SimConnect event: ") + name);
        return false;
    }
    return true;
}

bool SimConnectSession::MapClientEvent(SIMCONNECT_CLIENT_EVENT_ID eventId, const char *name)
{
    if (!m_handle || FAILED(SimConnect_MapClientEventToSimEvent(m_handle, eventId, name))) {
        m_log(std::string("Failed to map SimConnect event: ") + name);
        return false;
    }
    return true;
}

SendResult SimConnectSession::RequestObjectsByType(SIMCONNECT_DATA_REQUEST_ID requestId,
                                                   SIMCONNECT_DATA_DEFINITION_ID definitionId,
                                                   DWORD radiusMeters,
                                                   SIMCONNECT_SIMOBJECT_TYPE type)
{
    return Capture(m_handle ? SimConnect_RequestDataOnSimObjectType(
                                  m_handle, requestId, definitionId, radiusMeters, type)
                            : E_HANDLE);
}

SendResult SimConnectSession::RequestObjectData(SIMCONNECT_DATA_REQUEST_ID requestId,
                                                SIMCONNECT_DATA_DEFINITION_ID definitionId,
                                                DWORD objectId, SIMCONNECT_PERIOD period,
                                                DWORD interval)
{
    return Capture(m_handle ? SimConnect_RequestDataOnSimObject(
                                  m_handle, requestId, definitionId, objectId, period,
                                  SIMCONNECT_DATA_REQUEST_FLAG_DEFAULT, 0, interval, 0)
                            : E_HANDLE);
}

SendResult SimConnectSession::EnumerateObjects(SIMCONNECT_DATA_REQUEST_ID requestId,
                                               SIMCONNECT_SIMOBJECT_TYPE type)
{
    return Capture(m_handle ? SimConnect_EnumerateSimObjectsAndLiveries(m_handle, requestId, type)
                            : E_HANDLE);
}

SendResult SimConnectSession::TransmitEvent(DWORD objectId, SIMCONNECT_CLIENT_EVENT_ID eventId,
                                            DWORD data)
{
    return Capture(m_handle ? SimConnect_TransmitClientEvent(
                                  m_handle, objectId, eventId, data,
                                  SIMCONNECT_GROUP_PRIORITY_HIGHEST,
                                  SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY)
                            : E_HANDLE);
}

SendResult SimConnectSession::TransmitEventEx1(DWORD objectId,
                                               SIMCONNECT_CLIENT_EVENT_ID eventId,
                                               DWORD data0, DWORD data1)
{
    return Capture(m_handle ? SimConnect_TransmitClientEvent_EX1(
                                  m_handle, objectId, eventId,
                                  SIMCONNECT_GROUP_PRIORITY_HIGHEST,
                                  SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY,
                                  data0, data1)
                            : E_HANDLE);
}

SendResult SimConnectSession::SetObjectData(SIMCONNECT_DATA_DEFINITION_ID definitionId,
                                            DWORD objectId, DWORD arrayCount,
                                            DWORD elementSize, const void *data)
{
    return Capture(m_handle ? SimConnect_SetDataOnSimObject(
                                  m_handle, definitionId, objectId, 0, arrayCount, elementSize,
                                  const_cast<void *>(data))
                            : E_HANDLE);
}

SendResult SimConnectSession::CreateObject(std::string_view title,
                                           const SIMCONNECT_DATA_INITPOSITION &position,
                                           DWORD requestId)
{
    const std::string ownedTitle(title);
    return Capture(m_handle ? SimConnect_AICreateSimulatedObject_EX1(
                                  m_handle, ownedTitle.c_str(), "", position, requestId)
                            : E_HANDLE);
}

SendResult SimConnectSession::RemoveObject(DWORD objectId, DWORD requestId)
{
    return Capture(m_handle ? SimConnect_AIRemoveObject(m_handle, objectId, requestId) : E_HANDLE);
}

DWORD SimConnectSession::NextRequestId()
{
    return m_nextRequestId++;
}

void SimConnectSession::TrackOperation(const SendResult &send, DWORD requestId,
                                       std::string description,
                                       std::function<void()> onFailure)
{
    if (send.Succeeded() && send.packetId) {
        m_pendingOperations[*send.packetId] =
            PendingOperation{requestId, std::move(description), std::move(onFailure)};
    }
}

void SimConnectSession::CompleteRequest(DWORD requestId)
{
    std::erase_if(m_pendingOperations, [requestId](const auto &operation) {
        return operation.second.requestId == requestId;
    });
}

std::optional<std::string> SimConnectSession::HandleException(DWORD sendId)
{
    const auto operation = m_pendingOperations.find(sendId);
    if (operation == m_pendingOperations.end()) {
        return std::nullopt;
    }
    PendingOperation pending = std::move(operation->second);
    m_pendingOperations.erase(operation);
    if (pending.onFailure) {
        pending.onFailure();
    }
    return pending.description;
}

void CALLBACK SimConnectSession::DispatchThunk(SIMCONNECT_RECV *message, DWORD messageSize,
                                                void *context)
{
    auto &session = *static_cast<SimConnectSession *>(context);
    if (session.m_sink) {
        session.m_sink->OnSimConnectMessage(message, messageSize);
    }
}

SendResult SimConnectSession::Capture(HRESULT result) const
{
    SendResult captured{result};
    DWORD packetId = 0;
    if (m_handle && SUCCEEDED(result) && SUCCEEDED(SimConnect_GetLastSentPacketID(m_handle, &packetId))) {
        captured.packetId = packetId;
    }
    return captured;
}
} // namespace parking_services
