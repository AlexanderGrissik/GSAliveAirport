// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSBuggageLoader.h"
#include "../General/GSGeography.h"
#include "../General/GSCatalog.h"
#include "GSStatic.h"
#include "GSBuggageTrain.h"
#include "../Animation/GSAnimBFLO.h"
#include "../Animation/GSAnimSingle.h"
#include <algorithm>

namespace NS_GSAliveAirport
{

constexpr std::array GSDatums_BuggageLoaderExtStateGet{
    GSDatum{"BAGGAGELOADER END RAMP Y", "feet",   SIMCONNECT_DATATYPE_FLOAT32},
    GSDatum{"BAGGAGELOADER END RAMP Z", "meters", SIMCONNECT_DATATYPE_FLOAT32},
    GSDatum{"BAGGAGELOADER PIVOT Y",    "feet",   SIMCONNECT_DATATYPE_FLOAT32},
    GSDatum{"BAGGAGELOADER PIVOT Z",    "meters", SIMCONNECT_DATATYPE_FLOAT32},
};

constexpr std::array GSDatums_BuggageLoaderExtStateSet{
    GSDatum{"BAGGAGELOADER ANGLE TARGET", "degrees", SIMCONNECT_DATATYPE_FLOAT32}
};

void GSBuggageLoader::InitDatums(GSSimConnect& handler)
{
    handler.InvokeAddDatums(GSDatums_BuggageLoaderExtStateGet, GSDatumID_BuggageLoaderExtStateGet);
    handler.InvokeAddDatums(GSDatums_BuggageLoaderExtStateSet, GSDatumID_BuggageLoaderExtStateSet);
}

bool GSBuggageLoader::OnCreated()
{
    m_simHandle.PostReqCommand(new GSReqGetDataSimObj(m_simHandle, *this));
    return false;
}

void GSBuggageLoader::OnArrived()
{
    m_simHandle.PostReqIdentCommand(
        new GSReqTxEventEx1(m_simHandle, *this, GSEventID_OpenDoors, m_aircraft.GetObjID(), m_doorIdx + 1, 0),
        [](SIMCONNECT_RECV_EXCEPTION* message) {
            GSLogStream::LogDebug(GSLogStream::DBG_LOG_BUGGAGE_LOADER, "Ident Exception: OpenDoors, ") << message->dwException; });

    Freeze();

    if (m_ext) {
        GSCoord initPosLoc = { m_initPos.Longitude, m_initPos.Latitude };
        auto* anim = new GSAnimBFLO(m_simObjectID, initPosLoc);
        if (m_aircraft.IsDeparting()) {
            anim->AddFrameSet({ 4213, 0, 30.0 }, { 650, 0, 30.0 });
        } else {
            anim->AddFrameSet({ 0, 4213, 30.0 }, { 0, 650, 30.0 });
        }
        RegisterAnim(anim);

        auto ptrWorker = new GSStatic(m_simHandle, m_aircraft, *this);
        ptrWorker->SetTitle(GSCatalog::GetInstance().GetBuggageWorkerExt());
        ptrWorker->SetPosRel(GSStatic::REL_ABSOLUTE);
        ptrWorker->SetPosition(initPosLoc);
        ptrWorker->SetHeading(m_initPos.Heading);
        ptrWorker->GetInitPos().Altitude = m_initPos.Altitude;
        ptrWorker->GetInitPos().OnGround = 1;
        ptrWorker->PreSpawn();
        
        auto* animWorker = new GSAnimSingle(0, initPosLoc, GSDatumID_AnimWagonBLO);
        if (m_aircraft.IsDeparting()) {
            animWorker->AddFrameSet({ 4213, 0, 30.0 });
        } else {
            animWorker->AddFrameSet({ 0, 4213, 30.0 });
        }
        ptrWorker->SetAnimObj(animWorker);

        m_attached.emplace_back(ptrWorker);
    }

    auto ptrTrain = new GSBuggageTrain(m_simHandle, m_aircraft, *this, *this);
    ptrTrain->PreSpawn();
    m_attached.emplace_back(ptrTrain);
        
    ContinueSpawn();
}

void GSBuggageLoader::OnDespawning()
{
    m_simHandle.PostReqIdentCommand(new GSReqTxEventEx1(
        m_simHandle, *this, GSEventID_CloseDoors,
        m_aircraft.GetObjID(), m_doorIdx + 1, 0),
        [](SIMCONNECT_RECV_EXCEPTION* message) {
            GSLogStream::LogDebug(GSLogStream::DBG_LOG_BUGGAGE_LOADER, "Ident Exception: CloseDoors, ") << message->dwException; });
}

bool GSBuggageLoader::PreSpawn()
{
    auto cargoDoor = m_front ? m_aircraft.GetFrontCargoDoor() : m_aircraft.GetRearCargoDoor();
    if (!cargoDoor.has_value()) {
        return false;
    }

    m_doorIdx = static_cast<DWORD>(cargoDoor->second);

    m_title = GSCatalog::GetInstance().GetBuggageLoaderExt();
    if (m_title.empty()) {
        m_title = "Aso_Baggage_Loader_01";
        m_ext = false;
    }

    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    m_initPos.OnGround = 1;
    m_initPos.Heading = GSGeography::NormDeg(m_aircraft.GetHeading() + cargoDoor->first.get().headingDegrees + 180.0);
    m_initPos.Altitude = m_aircraft.GetGroundAltitudeFeet();

    const auto doorPos = GSGeography::RelativePosition(m_aircraft.GetHeading(), m_aircraft.GetLongLat(), cargoDoor->first.get().posZMeter, cargoDoor->first.get().posXMeter);
    m_initPos.Longitude = doorPos.Long();
    m_initPos.Latitude = doorPos.Lat();

    m_doorPosElev = m_aircraft.GetAltitudeAboveGroundFeet() + cargoDoor->first.get().posYFeet;

    PrepareRoute();

    return true;
}

void GSBuggageLoader::SetFinalPositionAndState(SIMCONNECT_RECV_SIMOBJECT_DATA& entry)
{
    StateWireDataGet rawStateGet;
    GSSimConnect::ReadMsgData(&rawStateGet, 0, sizeof(rawStateGet), entry);

    std::array updateData{ StateWireDataSet{ CalcLoaderOpenAngle(rawStateGet) } };
    m_simHandle.PostReqCommand(new GSReqSetDataSimObj(m_simHandle, *this, std::move(updateData)));

    const auto loaderPos = GSGeography::RelativePosition(m_initPos.Heading, { m_initPos.Longitude, m_initPos.Latitude }, -rawStateGet.endRampZMeters - 0.5, 0.0);
    m_initPos.Longitude = loaderPos.Long();
    m_initPos.Latitude = loaderPos.Lat();

    FinalizeRoute();
}

float GSBuggageLoader::CalcLoaderOpenAngle(const GSBuggageLoader::StateWireDataGet& rawStateGet)
{
    const float deltaYFeet = rawStateGet.endRampYFeet - rawStateGet.pivotYFeet;
    const float deltaZFeet = static_cast<float>((rawStateGet.endRampZMeters - rawStateGet.pivotZMeters) * GSGeography::FeetPerMeter);
    const float rampLengthFeet = std::hypot(deltaYFeet, deltaZFeet);

    const float desiredPhaseRadians = 
        std::asin(std::clamp((m_doorPosElev - rawStateGet.pivotYFeet) / rampLengthFeet, -1.0f, 1.0f));

    return static_cast<float>(desiredPhaseRadians * GSGeography::RadToDeg);
}
}
