// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSCateringCart.h"
#include "../Requests/GSReqCateringCartGetData.h"
#include "../Requests/GSReqCateringCartSetData.h"
#include "../General/GSGeography.h"
#include "../SimObjects/GSAircraft.h"

namespace NS_GSAliveAirport
{

constexpr std::array GSDatums_CateringTruckStateGet {
    GSDatum{"CATERINGTRUCK AIRCRAFT DOOR CONTACT OFFSET Z", "meters", SIMCONNECT_DATATYPE_FLOAT32},
};

constexpr std::array GSDatums_CateringTruckStateSet {
    GSDatum{"PLANE LONGITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64},
    GSDatum{"PLANE LATITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64},
    GSDatum{"CATERINGTRUCK ELEVATION TARGET", "feet", SIMCONNECT_DATATYPE_FLOAT32},
    GSDatum{"CATERINGTRUCK OPENING TARGET", "percent over 100", SIMCONNECT_DATATYPE_FLOAT32},
};

void GSCateringCart::InitDatums(GSSimConnect& handler)
{
    handler.InvokeAddDatums(GSDatums_CateringTruckStateGet, GSDatumID_CateringTruckStateGet);
    handler.InvokeAddDatums(GSDatums_CateringTruckStateSet, GSDatumID_CateringTruckStateSet);
}

bool GSCateringCart::OnCreated()
{
    m_simHandle.PostReqCommand(new GSReqCateringCartGetData(m_simHandle, *this));
    return false;
}

void GSCateringCart::OnArrived()
{
    std::array updateData{ StateWireDataSet{ m_initPos.Longitude, m_initPos.Latitude, m_doorPosElev, 1.0f } };
    m_simHandle.PostReqCommand(new GSReqCateringCartSetData(m_simHandle, *this, std::move(updateData)));

    Freeze();
    ContinueSpawn();
}

bool GSCateringCart::PreSpawn()
{
    auto rearRightDoor = m_aircraft.GetRearRightDoor();
    auto frontRightDoor = m_aircraft.GetFrontRightDoor();
    const auto* cateringDoor = (rearRightDoor.has_value() ? &rearRightDoor->first.get() : (frontRightDoor.has_value() ? &frontRightDoor->first.get() : nullptr));
    if (!cateringDoor) {
        return false;
    }

    m_title = "ASO_Catering_Truck_01";
    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    m_initPos.OnGround = 1;
    m_initPos.Heading = GSGeography::NormDeg(m_aircraft.GetHeading() + cateringDoor->headingDegrees + 180.0);
    m_initPos.Altitude = m_aircraft.GetGroundAltitudeFeet();
    
    const auto doorPos = GSGeography::RelativePosition(m_aircraft.GetHeading(), m_aircraft.GetLongLat(), cateringDoor->posZMeter, cateringDoor->posXMeter);
    m_initPos.Longitude = doorPos.Long();
    m_initPos.Latitude = doorPos.Lat();

    m_doorPosElev = m_aircraft.GetAltitudeAboveGroundFeet() + cateringDoor->posYFeet;
    
    PrepareRoute();

    return true;
}

void GSCateringCart::SetFinalPositionAndState(SIMCONNECT_RECV_SIMOBJECT_DATA& entry)
{
    StateWireDataGet rawStateGet;
    GSSimConnect::ReadMsgData(&rawStateGet, 0, sizeof(rawStateGet), entry);
    GSCoord out = GSGeography::RepositionZOffset({ m_initPos.Longitude, m_initPos.Latitude }, m_initPos.Heading, -rawStateGet.doorContactOffsetZ);
    m_initPos.Longitude = out.Long();
    m_initPos.Latitude = out.Lat();

    FinalizeRoute();
}
}
