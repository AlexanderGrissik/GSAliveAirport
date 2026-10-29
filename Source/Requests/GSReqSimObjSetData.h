// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimObjReq.h"
#include "../SimObjects/GSSimObj.h"
#include "../General/GSSimConnect.h"
#include "../General/GSLogStream.h"
#include <array>
#include <utility>

namespace NS_GSAliveAirport
{

template <typename T, DWORD N>
class GSReqSimObjSetData : public GSSimObjReq {
public:
    GSReqSimObjSetData(GSSimConnect& simHandle, GSSimObj& simObj, SIMCONNECT_DATA_DEFINITION_ID definitionID, std::array<T,N>&& data):
        GSSimObjReq(simHandle, simObj), m_definitionID(definitionID), m_data(std::forward<std::array<T,N>>(data)) {}
    GSReqRC Process() override;
    bool OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override { 
        (void)message;
        (void)messageSize;
        return true; 
    }
    void OnException(SIMCONNECT_RECV_EXCEPTION *message) override { (void)message; }
private:
    SIMCONNECT_DATA_DEFINITION_ID m_definitionID;
    std::array<T,N> m_data;
};


template <typename T, DWORD N>
GSReqRC GSReqSimObjSetData<T,N>::Process()
{
    auto simRC = m_simHandle.Invoke(
        SimConnect_SetDataOnSimObject, m_definitionID, m_simObj.GetSimObjectID(), 0, N, static_cast<DWORD>(sizeof(T)), m_data.data());
    if (!simRC.isOK()) {
        GSLogStream::LogError("GSReqSimObjSetData::Process Failed call: ") << simRC.rc;
    }

    return {simRC, false};
}

}
