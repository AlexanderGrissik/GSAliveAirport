// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSStaircase.h"
#include "../General/GSGeography.h"
#include "../General/GSCatalog.h"
#include "GSStatic.h"
#include <algorithm>

namespace NS_GSAliveAirport
{

constexpr std::array GSDatums_StaircaseExtStateGet{
    GSDefinitions::DatumSpec{"BOARDINGRAMP END POSITION Y",  "feet", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"BOARDINGRAMP END POSITION Z",  "meters", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"BOARDINGRAMP START POSITION Y","feet", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"BOARDINGRAMP START POSITION Z","meters", SIMCONNECT_DATATYPE_FLOAT32},
};

constexpr std::array GSDatums_StaircaseExtStateSet{
    GSDefinitions::DatumSpec{"BOARDINGRAMP ORIENTATION TARGET", "percent", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"BOARDINGRAMP ELEVATION TARGET", "feet", SIMCONNECT_DATATYPE_FLOAT32},
};

void GSStaircase::InitDatums(GSSimConnect& handler)
{
    handler.InvokeAddDatums(GSDatums_StaircaseExtStateGet, GSDefinitions::GSDefID::GSDefID_StaircaseExtStateGet);
    handler.InvokeAddDatums(GSDatums_StaircaseExtStateSet, GSDefinitions::GSDefID::GSDefID_StaircaseExtStateSet);
}

bool GSStaircase::OnCreated()
{
    m_simHandle.PostReqCommand(new GSReqGetDataSimObj(m_simHandle, *this));
    return false;
}

void GSStaircase::OnArrived()
{
    m_simHandle.PostReqIdentCommand(
        new GSReqTxEventEx1(m_simHandle, *this, GSDefinitions::GSDefID_OpenDoors, m_aircraft.GetObjID(), m_doorIdx + 1, 0),
        [](SIMCONNECT_RECV_EXCEPTION* message) {
            GSLogStream::LogDebug(GSLogStream::DBG_LOG_BUGGAGE_LOADER, "Ident Exception: GSDefID_OpenDoors, ") << message->dwException; });

    Freeze();

    /*if (m_ext) {
        auto* anim = new GSAnimBFLO(m_simObjectID);
        anim->AddFrameSet(4213, 0, 30.0, 650, 0, 30.0);
        RegisterAnim(anim);

        auto ptrWorker = new GSStatic(m_simHandle, m_aircraft, *this);
        ptrWorker->SetTitle(GSCatalog::GetInstance().GetBuggageWorkerExt());
        ptrWorker->SetPosRel(GSStatic::REL_ABSOLUTE);
        ptrWorker->SetPosition({ m_initPos.Longitude, m_initPos.Latitude });
        ptrWorker->SetHeading(m_initPos.Heading);
        ptrWorker->GetInitPos().Altitude = m_initPos.Altitude;
        ptrWorker->GetInitPos().OnGround = 1;
        ptrWorker->PreSpawn();
        
        auto* animWorker = new GSAnimSingle(0);
        animWorker->AddFrameSet(GSDefinitions::GSDefID_AnimWagonBLO, 4213, 0, 30.0);
        ptrWorker->SetAnimObj(animWorker);

        m_attached.emplace_back(ptrWorker);
    }

    auto ptrTrain = new GSBuggageTrain(m_simHandle, m_aircraft, *this, *this);
    ptrTrain->PreSpawn();
    m_attached.emplace_back(ptrTrain);*/
        
    ContinueSpawn();
}

void GSStaircase::OnDespawning()
{
    m_simHandle.PostReqIdentCommand(new GSReqTxEventEx1(
        m_simHandle, *this, GSDefinitions::GSDefID_CloseDoors,
        m_aircraft.GetObjID(), m_doorIdx + 1, 0),
        [](SIMCONNECT_RECV_EXCEPTION* message) {
            GSLogStream::LogDebug(GSLogStream::DBG_LOG_STAIRCASE, "Ident Exception: GSDefID_CloseDoors, ") << message->dwException; });
}

bool GSStaircase::PreSpawn()
{
    const auto& airData = m_aircraft.GetRawData();
    auto paxDoor = m_front ? m_aircraft.GetFrontLeftDoor() : m_aircraft.GetRearLeftDoor();
    if (!paxDoor.has_value()) {
        return false;
    }

    m_doorIdx = static_cast<DWORD>(paxDoor->second);

    m_title = GSCatalog::GetInstance().GetStaircaseExt();
    if (m_title.empty()) {
        m_title = "ASO_Boarding_Stairs";
        m_ext = false;
    }

    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    m_initPos.OnGround = 1;
    m_initPos.Heading = GSGeography::NormDeg(airData.headingDegrees + paxDoor->first.get().headingDegrees + 180.0);
    m_initPos.Altitude = airData.groundAltitudeFeet;

    const auto doorPos = GSGeography::RelativePosition(airData.headingDegrees, m_aircraft.GetLongLat(), paxDoor->first.get().posZMeter, paxDoor->first.get().posXMeter);
    m_initPos.Longitude = doorPos.Long();
    m_initPos.Latitude = doorPos.Lat();

    m_doorPosElev = airData.alt_abv_grnd + paxDoor->first.get().posYFeet; 

    PrepareRoute();

    return true;
}

void GSStaircase::SetFinalPositionAndState(SIMCONNECT_RECV_SIMOBJECT_DATA& entry)
{
    StateWireDataGet rawStateGet;
    GSSimConnect::ReadMsgData(&rawStateGet, sizeof(rawStateGet), entry);

    // Clamp if the door is beneath or above the Staircase Y range.
    const float yRelation = std::clamp(
        (m_doorPosElev - rawStateGet.rampStartPosYFeet) / (rawStateGet.rampEndPosYFeet - rawStateGet.rampStartPosYFeet),
        0.0f, 1.0f);

    // Interpolated extentions/orientation Z according to yRelation.
    const float extZMeters = rawStateGet.rampStartPosZMeters + yRelation * (rawStateGet.rampEndPosZMeters - rawStateGet.rampStartPosZMeters);

    std::array updateData{ StateWireDataSet{ yRelation * 100.0f, m_doorPosElev } };
    m_simHandle.PostReqCommand(new GSReqSetDataSimObj(m_simHandle, *this, std::move(updateData)));

    const auto stairPos = GSGeography::RelativePosition(
        m_initPos.Heading, { m_initPos.Longitude, m_initPos.Latitude }, -extZMeters - 0.1, 0.0);
    m_initPos.Longitude = stairPos.Long();
    m_initPos.Latitude  = stairPos.Lat();

    FinalizeRoute();
}

}