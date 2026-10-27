// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimObj.h"
#include "../General/GSDefinitions.h"
#include "../General/GSLogStream.h"
#include <array>
#include <cstddef>
#include <utility>

namespace NS_GSAliveAirport
{
class GSStaircase : public GSSimObj
{
public:

    #pragma pack(push, 1)
    struct StateWireDataGet
    {
        float rampEndPosYFeet;
        float rampEndPosZMeters;
        float rampStartPosYFeet;
        float rampStartPosZMeters;
    };
    struct StateWireDataSet
    {
        float targetOrientationPerc;
        float targetElevationFeet;
    };
    #pragma pack(pop)

    GSStaircase(GSSimConnect& simHandle, GSAircraft& aircraft, GSSimObjUpdate& parent, bool front) :
        GSSimObj(simHandle, aircraft, parent), m_front(front) {}
        
    static void InitDatums(GSSimConnect& handler);

    void SetFinalPositionAndState(SIMCONNECT_RECV_SIMOBJECT_DATA& entry);

private:

    bool PreSpawn() override;
    bool OnCreated() override;
    void OnArrived() override;
    void OnDespawning() override;
    void OnSpawned(bool ok, GSSimObj& obj) override;

    void TrySpawnPassengers();

    GSCoord m_doorPos;
    GSCoord m_stairBasePos;
    GSCoord m_stairTopPos;
    float m_doorPosElev = 0.0f;
    DWORD m_doorIdx = 0;
    bool m_front = false;
    bool m_stairsArrived = false;
    bool m_busArrived = false;
    bool m_paxSpawned = false;

    class GSReqGetDataSimObj : public GSSimObj::GSReqGetDataSimObj {
    public:
        GSReqGetDataSimObj(GSSimConnect& simHandle, GSSimObj& simObj) :
            GSSimObj::GSReqGetDataSimObj(simHandle, simObj, GSDefinitions::GSDefID::GSDefID_StaircaseExtStateGet) {}

        bool OnMessage(SIMCONNECT_RECV* message, DWORD messageSize) override {
            (void)messageSize;
            if (message->dwID == SIMCONNECT_RECV_ID_SIMOBJECT_DATA) {
                static_cast<GSStaircase&>(m_simObj).SetFinalPositionAndState(*static_cast<SIMCONNECT_RECV_SIMOBJECT_DATA*>(message));
            } else {
                GSLogStream::LogError("GSStaircase::GSReqGetDataSimObj::OnMessage Unexpected Message: ") << message->dwID;
                m_simObj.OnObjSpawned(true);
            }
            return true;
        }

        void OnException(SIMCONNECT_RECV_EXCEPTION* message) override {
            m_simObj.OnObjSpawned(true);
            GSLogStream::LogError("GSStaircase::GSReqGetDataSimObj::OnException: ") << message->dwException << ", " << message->dwIndex;
        }
    };

    class GSReqSetDataSimObj : public GSSimObj::GSReqSetDataSimObj<StateWireDataSet, 1> {
    public:
        GSReqSetDataSimObj(GSSimConnect& simHandle, GSSimObj& simObj, std::array<StateWireDataSet, 1>&& data) :
            GSSimObj::GSReqSetDataSimObj<StateWireDataSet, 1>(
                simHandle, simObj, GSDefinitions::GSDefID::GSDefID_StaircaseExtStateSet,
                std::forward<std::array<StateWireDataSet, 1>>(data)) {
        }
    };
};

}