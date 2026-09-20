#include "GSCateringCart.h"
#include "../General/GSGeography.h"
#include "../SimObjects/GSAircraft.h"

namespace NS_GSLiveAirportMSFS
{

constexpr std::array GSDatums_CateringTruckStateGet {
    GSDefinitions::DatumSpec{"CATERINGTRUCK AIRCRAFT DOOR CONTACT OFFSET Z", "meters", SIMCONNECT_DATATYPE_FLOAT32},
};

constexpr std::array GSDatums_CateringTruckStateSet {
    GSDefinitions::DatumSpec{"PLANE LONGITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64},
    GSDefinitions::DatumSpec{"PLANE LATITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64},
    GSDefinitions::DatumSpec{"CATERINGTRUCK ELEVATION TARGET", "feet", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"CATERINGTRUCK OPENING TARGET", "percent over 100", SIMCONNECT_DATATYPE_FLOAT32},
};

void GSCateringCart::InitDatums(GSSimConnect& handler)
{
    handler.InvokeAddDatums(GSDatums_CateringTruckStateGet, GSDefinitions::GSDefID::GSDefID_CateringTruckStateGet);
    handler.InvokeAddDatums(GSDatums_CateringTruckStateSet, GSDefinitions::GSDefID::GSDefID_CateringTruckStateSet);
}

bool GSCateringCart::OnCreated()
{
    m_simHandle.PostReqCommand(new GSReqGetDataSimObj(m_simHandle, *this));
    return false;
}

void GSCateringCart::OnArrived()
{
    std::array updateData{ StateWireDataSet{ m_initPos.Longitude, m_initPos.Latitude, m_doorPosElev, 1.0f } };
    m_simHandle.PostReqCommand(new GSReqSetDataSimObj(m_simHandle, *this, std::move(updateData)));

    Freeze();
    ContinueSpawn();
}

bool GSCateringCart::PreSpawn()
{
    const auto& airData = m_aircraft.GetRawData();
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
    m_initPos.Heading = GSGeography::NormDeg(airData.headingDegrees + cateringDoor->headingDegrees + 180.0);
    m_initPos.Altitude = airData.groundAltitudeFeet;
    
    const auto doorPos = GSGeography::RelativePosition(airData.headingDegrees, m_aircraft.GetLongLat(), cateringDoor->posZMeter, cateringDoor->posXMeter);
    m_initPos.Longitude = doorPos.Long();
    m_initPos.Latitude = doorPos.Lat();

    m_doorPosElev = airData.alt_abv_grnd + cateringDoor->posYFeet;
    
    PrepareRoute();

    return true;
}

void GSCateringCart::SetFinalPositionAndState(SIMCONNECT_RECV_SIMOBJECT_DATA& entry)
{
    StateWireDataGet rawStateGet;
    GSSimConnect::ReadMsgData(&rawStateGet, sizeof(rawStateGet), entry);
    GSCoord out = GSGeography::RepositionZOffset({ m_initPos.Longitude, m_initPos.Latitude }, m_initPos.Heading, -rawStateGet.doorContactOffsetZ);
    m_initPos.Longitude = out.Long();
    m_initPos.Latitude = out.Lat();

    FinalizeRoute();
}
}
