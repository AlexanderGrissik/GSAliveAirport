// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSStaircase.h"
#include <chrono>
#include "../General/GSGeography.h"
#include "../General/GSCatalog.h"
#include "../General/GSRandom.h"
#include "GSPassenger.h"
#include "GSPaxBus.h"
#include <algorithm>
#include <iterator>

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

void GSStaircase::OnSpawned(bool ok, GSSimObj& obj)
{
    if (m_paxSpawned) {
        GSSimObj::OnSpawned(ok, obj);
    } else {
        ++m_childSpawned;
        TrySpawnPassengers();
    }
}

void GSStaircase::OnArrived()
{
    m_simHandle.PostReqIdentCommand(
        new GSReqTxEventEx1(m_simHandle, *this, GSDefinitions::GSDefID_OpenDoors, m_aircraft.GetObjID(), m_doorIdx + 1, 0),
        [](SIMCONNECT_RECV_EXCEPTION* message) {
            GSLogStream::LogDebug(GSLogStream::DBG_LOG_BUGGAGE_LOADER, "Ident Exception: GSDefID_OpenDoors, ") << message->dwException; });

    Freeze();

    m_stairsArrived = true;
    TrySpawnPassengers();
}

void GSStaircase::TrySpawnPassengers()
{
    static const size_t PASSANGER_COUNT = 8U;
    static constexpr std::chrono::seconds DELTA_SPAWN{3};

    if (!m_stairsArrived || !m_busArrived || m_paxSpawned) {
        return;
    }

    float totDoorAltFeet = m_aircraft.GetGroundAltitudeFeet() + m_doorPosElev;
    float stairBaseHeading = static_cast<float>(GSGeography::NormDeg(m_initPos.Heading + 180.0));

    GSTimePoint incrDelta = std::chrono::steady_clock::now();
    std::generate_n(std::back_inserter(m_attached), PASSANGER_COUNT, [this, stairBaseHeading, totDoorAltFeet, &incrDelta] {
        auto passenger = std::make_unique<GSPassenger>(
            m_simHandle, m_aircraft, *this, m_stairBasePos, m_stairTopPos, m_doorPos, stairBaseHeading, totDoorAltFeet);
        passenger->SetDeferredSpawn(incrDelta);
        passenger->PreSpawn();
        incrDelta += DELTA_SPAWN;
        return passenger;
    });
        
    m_paxSpawned = true;
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
    auto paxDoor = m_front ? m_aircraft.GetFrontLeftDoor() : m_aircraft.GetRearLeftDoor();
    if (!paxDoor.has_value()) {
        return false;
    }

    m_doorIdx = static_cast<DWORD>(paxDoor->second);

    m_title = "ASO_Boarding_Stairs";

    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    m_initPos.OnGround = 1;
    m_initPos.Heading = GSGeography::NormDeg(m_aircraft.GetHeading() + paxDoor->first.get().headingDegrees + 180.0);
    m_initPos.Altitude = m_aircraft.GetGroundAltitudeFeet();

    m_doorPos = GSGeography::RelativePosition(m_aircraft.GetHeading(), m_aircraft.GetLongLat(), paxDoor->first.get().posZMeter, paxDoor->first.get().posXMeter);
    m_initPos.Longitude = m_doorPos.Long();
    m_initPos.Latitude = m_doorPos.Lat();

    m_doorPosElev = m_aircraft.GetAltitudeAboveGroundFeet() + paxDoor->first.get().posYFeet; 

    PrepareRoute();

    return true;
}

void GSStaircase::SetFinalPositionAndState(SIMCONNECT_RECV_SIMOBJECT_DATA& entry)
{
    StateWireDataGet rawStateGet;
    GSSimConnect::ReadMsgData(&rawStateGet, 0, sizeof(rawStateGet), entry);

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

    m_stairBasePos = GSGeography::RelativePosition(
        m_initPos.Heading, { m_initPos.Longitude, m_initPos.Latitude }, -5.1, 0.0);
    
    m_stairTopPos = GSGeography::RelativePosition(
        m_initPos.Heading, { m_initPos.Longitude, m_initPos.Latitude }, extZMeters - 3.2, 0.0);

    FinalizeRoute();

    auto halfWing = m_aircraft.GetWingSpanMeters() / 5.0;
    double distY = GSRandom::RandDouble(halfWing, halfWing + 1.0);
    auto busPos = GSGeography::RelativePosition(m_initPos.Heading, m_stairBasePos, -distY - 3, 0.0);
    
    auto bus = std::make_unique<GSPaxBus>(
        m_simHandle, m_aircraft, *this, busPos, GSGeography::NormDeg(m_initPos.Heading + 90.0), m_busArrived);
    if (bus->PreSpawn()) {
        m_attached.emplace_back(std::move(bus));
        ContinueSpawn();
    }
}

}
