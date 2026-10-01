// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSSimConnect.h"
#include "GSLogStream.h"
#include "GSDefinitions.h"
#include <chrono>
#include <cstring>
#include <thread>
#include <array>
#include <vector>

using namespace std::chrono_literals;

namespace NS_GSAliveAirport
{

template <typename T>
DWORD SMReq(SIMCONNECT_RECV *message)
{
    return static_cast<T*>(message)->dwRequestID;
}

DWORD SMReqF(SIMCONNECT_RECV *message)
{
    return static_cast<SIMCONNECT_RECV_FACILITY_DATA*>(message)->UserRequestId;
}

DWORD SMReqFE(SIMCONNECT_RECV *message)
{
    return static_cast<SIMCONNECT_RECV_FACILITY_DATA_END*>(message)->RequestId;
}

DWORD SMReqA(SIMCONNECT_RECV *message)
{
    return static_cast<SIMCONNECT_RECV_ACTION_CALLBACK*>(message)->cbRequestId;
}

using ReqIdGet = DWORD (*)(SIMCONNECT_RECV*);
constexpr std::array s_hasRequestID = [] { 
    constexpr ReqIdGet values[] {
        nullptr,                                                       //  0 SIMCONNECT_RECV_ID_NULL
        nullptr,                                                       //  1 SIMCONNECT_RECV_ID_EXCEPTION
        nullptr,                                                       //  2 SIMCONNECT_RECV_ID_OPEN
        nullptr,                                                       //  3 SIMCONNECT_RECV_ID_QUIT
        nullptr,                                                       //  4 SIMCONNECT_RECV_ID_EVENT
        nullptr,                                                       //  5 SIMCONNECT_RECV_ID_EVENT_OBJECT_ADDREMOVE
        nullptr,                                                       //  6 SIMCONNECT_RECV_ID_EVENT_FILENAME
        nullptr,                                                       //  7 SIMCONNECT_RECV_ID_EVENT_FRAME
        SMReq<SIMCONNECT_RECV_SIMOBJECT_DATA>,                         //  8 SIMCONNECT_RECV_ID_SIMOBJECT_DATA            (dwRequestID)
        SMReq<SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE>,                  //  9 SIMCONNECT_RECV_ID_SIMOBJECT_DATA_BYTYPE     (via SIMOBJECT_DATA)
        SMReq<SIMCONNECT_RECV_WEATHER_OBSERVATION>,                    // 10 SIMCONNECT_RECV_ID_WEATHER_OBSERVATION        (dwRequestID)
        SMReq<SIMCONNECT_RECV_CLOUD_STATE>,                            // 11 SIMCONNECT_RECV_ID_CLOUD_STATE                (dwRequestID)
        SMReq<SIMCONNECT_RECV_ASSIGNED_OBJECT_ID>,                     // 12 SIMCONNECT_RECV_ID_ASSIGNED_OBJECT_ID         (dwRequestID)
        nullptr,                                                       // 13 SIMCONNECT_RECV_ID_RESERVED_KEY               (none in this header)
        nullptr,                                                       // 14 SIMCONNECT_RECV_ID_CUSTOM_ACTION              (none; inherits EVENT)
        SMReq<SIMCONNECT_RECV_SYSTEM_STATE>,                           // 15 SIMCONNECT_RECV_ID_SYSTEM_STATE               (dwRequestID)
        SMReq<SIMCONNECT_RECV_CLIENT_DATA>,                            // 16 SIMCONNECT_RECV_ID_CLIENT_DATA                (via SIMOBJECT_DATA)
        nullptr,                                                       // 17 SIMCONNECT_RECV_ID_EVENT_WEATHER_MODE
        SMReq<SIMCONNECT_RECV_FACILITIES_LIST>,                        // 18 SIMCONNECT_RECV_ID_AIRPORT_LIST               (via FACILITIES_LIST)
        SMReq<SIMCONNECT_RECV_FACILITIES_LIST>,                        // 19 SIMCONNECT_RECV_ID_VOR_LIST                   (via FACILITIES_LIST)
        SMReq<SIMCONNECT_RECV_FACILITIES_LIST>,                        // 20 SIMCONNECT_RECV_ID_NDB_LIST                   (via FACILITIES_LIST)
        SMReq<SIMCONNECT_RECV_FACILITIES_LIST>,                        // 21 SIMCONNECT_RECV_ID_WAYPOINT_LIST              (via FACILITIES_LIST)
        nullptr,                                                       // 22 SIMCONNECT_RECV_ID_EVENT_MULTIPLAYER_SERVER_STARTED
        nullptr,                                                       // 23 SIMCONNECT_RECV_ID_EVENT_MULTIPLAYER_CLIENT_STARTED
        nullptr,                                                       // 24 SIMCONNECT_RECV_ID_EVENT_MULTIPLAYER_SESSION_ENDED
        nullptr,                                                       // 25 SIMCONNECT_RECV_ID_EVENT_RACE_END
        nullptr,                                                       // 26 SIMCONNECT_RECV_ID_EVENT_RACE_LAP
        nullptr,                                                       // 27 SIMCONNECT_RECV_ID_EVENT_EX1
        SMReqF,                                                        // 28 SIMCONNECT_RECV_ID_FACILITY_DATA              
        SMReqFE,                                                       // 29 SIMCONNECT_RECV_ID_FACILITY_DATA_END          
        SMReq<SIMCONNECT_RECV_LIST_TEMPLATE>,                          // 30 SIMCONNECT_RECV_ID_FACILITY_MINIMAL_LIST      (via LIST_TEMPLATE)
        SMReq<SIMCONNECT_RECV_LIST_TEMPLATE>,                          // 31 SIMCONNECT_RECV_ID_JETWAY_DATA                (via LIST_TEMPLATE)
        SMReq<SIMCONNECT_RECV_LIST_TEMPLATE>,                          // 32 SIMCONNECT_RECV_ID_CONTROLLERS_LIST           (via LIST_TEMPLATE)
        SMReqA,                                                        // 33 SIMCONNECT_RECV_ID_ACTION_CALLBACK            
        SMReq<SIMCONNECT_RECV_LIST_TEMPLATE>,                          // 34 SIMCONNECT_RECV_ID_ENUMERATE_INPUT_EVENTS     (via LIST_TEMPLATE)
        SMReq<SIMCONNECT_RECV_GET_INPUT_EVENT>,                        // 35 SIMCONNECT_RECV_ID_GET_INPUT_EVENT            (dwRequestID)
        nullptr,                                                       // 36 SIMCONNECT_RECV_ID_SUBSCRIBE_INPUT_EVENT      (none; only Hash)
        nullptr,                                                       // 37 SIMCONNECT_RECV_ID_ENUMERATE_INPUT_EVENT_PARAMS (none; only Hash)
        SMReq<SIMCONNECT_RECV_LIST_TEMPLATE>,                          // 38 SIMCONNECT_RECV_ID_ENUMERATE_SIMOBJECT_AND_LIVERY_LIST (via LIST_TEMPLATE)
        nullptr,                                                       // 39 SIMCONNECT_RECV_ID_FLOW_EVENT                 (none in this header)
        nullptr,                                                       // 40 SIMCONNECT_RECV_ID_CAMERA_DATA                (none in this header)
        nullptr,                                                       // 41 SIMCONNECT_RECV_ID_CAMERA_STATUS              (none in this header)
        SMReq<SIMCONNECT_RECV_LIST_TEMPLATE>,                          // 42 SIMCONNECT_RECV_ID_CAMERA_DEFINITION_LIST     (via LIST_TEMPLATE)
        SMReq<SIMCONNECT_RECV_LIST_TEMPLATE>,                          // 43 SIMCONNECT_RECV_ID_COMM_BUS                   (via LIST_TEMPLATE)
        nullptr,                                                       // 44 SIMCONNECT_RECV_ID_CAMERA_WORLD_LOCKER        (none in this header)
    };
    return std::to_array(values);
}();

void GSSimConnect::RunDispatch(std::stop_token stopToken)
{
    m_lastDispatch = true;
    m_lastLoopMsg = false;
    while (!stopToken.stop_requested() && m_lastDispatch) {
        if (!m_handle) {
            Connect();
        }

        if (m_handle) {
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

        RunCommands();
        if (!m_handle) {
            std::this_thread::sleep_for(2000ms);
        }
    }
}

void GSSimConnect::Connect()
{
    if (!m_handle) {
        if (FAILED(SimConnect_Open(&m_handle, "GSAliveAirport", nullptr, 0, nullptr, 0))) {
            m_handle = nullptr;
        }

        if (m_handle && FAILED(SimConnect_SubscribeToSystemEvent(m_handle, GSDefinitions::GSEventID_SimState, "Sim"))) {
            GSLogStream::Log(std::string("Failed to subscribe to SimConnect event: Sim"));
            SimConnect_Close(m_handle);
            m_handle = nullptr;
            return;
        }

        if (!m_handle) {
            if (!m_reportedWaiting) {
                GSLogStream::Log("Waiting for Microsoft Flight Simulator 2024...");
                m_reportedWaiting = true;
            }
            return;
        }
    }

    GSLogStream::Log("Connected to MSFS 2024.");
    m_reportedWaiting = false;
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
    m_lastLoopMsg = true;
    switch (message->dwID) {
    case SIMCONNECT_RECV_ID_OPEN:
        break;
    case SIMCONNECT_RECV_ID_QUIT:
        GSLogStream::Log("MSFS24 closed the SimConnect connection.");
        m_disconnectRequested = true;
        OnDisconnect();
        break;
    case SIMCONNECT_RECV_ID_EVENT: {
        const auto &event = *reinterpret_cast<SIMCONNECT_RECV_EVENT *>(message);
        if (event.uEventID == GSDefinitions::GSEventID_SimState) {
            if (event.dwData != 0) {
                GSLogStream::Log("Simulation detected.");
                m_simStarted = true;
                OnSimStart();
            } else {
                GSLogStream::Log("Simulation stopped.");
                m_simStarted = false;
                FlushPendingRequestsWithException();
                OnSimStop();
            }
        } else {
            GSLogStream::Log("Unexpected Event: ") << event.uEventID;
        }
        break;
    }
    case SIMCONNECT_RECV_ID_EXCEPTION: {
        auto& msg = *static_cast<SIMCONNECT_RECV_EXCEPTION*>(message);
        GSDefinitions::SendResultIDs id{msg.dwSendID, 0};
        auto itr = m_sendIDToPtr.find(id);
        if (itr != m_sendIDToPtr.end()) {
            itr->second->OnException(&msg);
            id = itr->first;
            m_sendIDToPtr.erase(itr);
            m_reqIDToPtr.erase(id);
        } else {
            GSLogStream::Log("GSSimConnect - Unexpected exception: ") << GetDebugName() << ", Exception: "
                << msg.dwException << ", sendID: " << msg.dwSendID;
        }
        break;
    }
    default:
        if (message->dwID < s_hasRequestID.size() && s_hasRequestID[message->dwID]) {
            GSDefinitions::SendResultIDs id{0, s_hasRequestID[message->dwID](message)};
            auto itr = m_reqIDToPtr.find(id);
            if (itr != m_reqIDToPtr.end()) {
                if (itr->second.get().OnMessage(message, messageSize)) {
                    id = itr->first;
                    m_reqIDToPtr.erase(itr);
                    m_sendIDToPtr.erase(id);
                }
            } else {
                GSLogStream::LogError("GSSimConnect - Missing request: ") << message->dwID << ", Size: " << messageSize;
            }
        } else {
            GSLogStream::Log("GSSimConnect - Unexpected message: ") << message->dwID << ", Size: " << messageSize;
        }
        break;
    }

    m_lastDispatch = true;
}

void CALLBACK GSSimConnect::DispatchThunk(SIMCONNECT_RECV *message, DWORD messageSize, void *context)
{
    reinterpret_cast<GSSimConnect *>(context)->OnSimConnectMessage(message, messageSize);
}

void GSSimConnect::InvokeAddDatum(SIMCONNECT_DATA_DEFINITION_ID DefineID, const char * DatumName, const char * UnitsName, SIMCONNECT_DATATYPE DatumType)
{
    if (!CaptureResult(SimConnect_AddToDataDefinition(m_handle, DefineID, DatumName, UnitsName, DatumType), 0).isOK()) {
        GSLogStream::LogError("Unable to add definitions for: ") << 
            DefineID << ", Name: " << DatumName << ", Units: " << UnitsName << ", Type: " << DatumType;
    }
}

void GSSimConnect::InvokeAddFacilityDatum(SIMCONNECT_DATA_DEFINITION_ID DefineID, const char* DatumName)
{
    if (!CaptureResult(SimConnect_AddToFacilityDefinition(m_handle, DefineID, DatumName), 0).isOK()) {
        GSLogStream::LogError("Unable to add definitions for: ") << DefineID << ", Name: " << DatumName;
    }
}

void GSSimConnect::InvokeMapClientEvent(SIMCONNECT_CLIENT_EVENT_ID EventID, const char * EventName)
{
    if (!CaptureResult(SimConnect_MapClientEventToSimEvent(m_handle, EventID, EventName), 0).isOK()) {
        GSLogStream::LogError("Unable to map client event for: ") << EventID << ", Name: " << EventName;
    }
}

void GSSimConnect::RunCommands()
{
    bool cont = true;
    do {
        
        auto opt = m_commands.TryPop();
        if (opt.has_value()) {
            m_lastLoopMsg = true;
            if (opt->get()->GetCmdID() == GSDefinitions::CMD_REQ_PROCESS)
                HandleCmdReqProcess(static_cast<GSCmdReq&>(*opt->get()));
            else
                OnCommand(*opt->get());
        } else {
            cont = false;
        }
    } while (cont);
}

void GSSimConnect::HandleCmdReqProcess(GSCmdReq& cmd)
{
    auto req(cmd.ReleaseReq());
    GSRequest::SendResult rc = req->Process();
    if (rc.m_simRC.isOK() && rc.m_keep) {
        m_reqIDToPtr.emplace(rc.m_simRC, *(req.get()));
        m_sendIDToPtr.emplace(rc.m_simRC, std::move(req));
    } else if (!rc.m_simRC.isOK()) {
        GSLogStream::LogError("Command Post Failed");
    }
}

GSDefinitions::SendResult GSSimConnect::CaptureResult(HRESULT result, DWORD requestID) const
{
    DWORD sendId = 0;
    if (GetHandle() && SUCCEEDED(result)) {
        if (!SUCCEEDED(SimConnect_GetLastSentPacketID(GetHandle(), &sendId))) {
            sendId = 0;
        }
    }
    return {sendId, requestID, result};
}

void GSSimConnect::ReadMsgData(void* dest, size_t destSize, const SIMCONNECT_RECV_SIMOBJECT_DATA& entry)
{
    const auto* begin = reinterpret_cast<const BYTE*>(&entry);
    const auto* data = reinterpret_cast<const BYTE*>(&entry.dwData);
    const size_t offset = data - begin;
    const size_t payloadSize = entry.dwSize - offset;
    std::memcpy(dest, data, std::min(payloadSize, destSize));
}

void GSSimConnect::PostReqCommand(GSRequest* req)
{
    GSCmdReq::ReqPtr reqPtr(req);
    CmdPtr cmd(new GSCmdReq(GSDefinitions::CMD_REQ_PROCESS, reqPtr));
    PostCommand(cmd);
}

void GSSimConnect::FlushPendingRequestsWithException()
{
    if (m_sendIDToPtr.empty()) return;
    
    std::vector<GSRequest*> toFlush;
    toFlush.reserve(m_sendIDToPtr.size());
    for (auto& [key, req] : m_sendIDToPtr) {
        toFlush.push_back(req.get());
    }
    
    SIMCONNECT_RECV_EXCEPTION exc{};
    exc.dwID = SIMCONNECT_RECV_ID_EXCEPTION;
    for (auto* req : toFlush) {
        req->OnException(&exc);
    }
    
    m_sendIDToPtr.clear();
    m_reqIDToPtr.clear();
    
    GSLogStream::Log("GSSimConnect - SimStop: flushed ") << toFlush.size() << " pending request(s) for: " << GetDebugName();
}

} // namespace NS_GSAliveAirport
