#include "GSGroundPowerMSFS.h"
#include "../GSGeography.h"

namespace NS_GSLiveAirportMSFS
{

constexpr std::array GSDatums_CateringTruckStateSet {
    GSDefinitions::DatumSpec{"GROUNDPOWERUNIT HOSE DEPLOYED", "percent over 100", SIMCONNECT_DATATYPE_FLOAT32},
};

void GSGroundPowerMSFS::InitDatums(GSSimConnect& handler)
{
    handler.InvokeAddDatums(GSDatums_CateringTruckStateSet, GSDefinitions::GSDefID::GSDefID_GroundPowerStateSet);
}

void GSGroundPowerMSFS::OnCreated()
{
    SetFinalPositionAndState();
    OnSpawned(true);
}

bool GSGroundPowerMSFS::PreSpawn()
{
    const auto& airData = m_aircraft.GetRawData();
    auto groundPowerDoor = m_aircraft.GetGroundPowerDoor();
    auto frontLeftDoor = m_aircraft.GetFrontLeftDoor();
    const auto* powerDoor = (groundPowerDoor.has_value() ? &groundPowerDoor->get() : (frontLeftDoor.has_value() ? &frontLeftDoor->get() : nullptr));
    if (!powerDoor) {
        return false;
    }

    m_title = "Car Ground Power Unit";
    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    m_initPos.OnGround = 1;
    m_initPos.Heading = GSGeography::NormDeg(airData.headingDegrees + 180.0);
    m_initPos.Altitude = airData.groundAltitudeFeet;
    
    const auto doorPos = GSGeography::RelativePosition(
        airData.headingDegrees, m_aircraft.GetLongLat(), powerDoor->posZMeter + 10, powerDoor->posXMeter);
    m_initPos.Longitude = doorPos.Long();
    m_initPos.Latitude = doorPos.Lat();

    return true;
}

void GSGroundPowerMSFS::SetFinalPositionAndState()
{
    std::array updateData{ StateWireDataSet{ 1.0f } };
    m_simHandle.PostReqCommand(new GSReqSetDataSimObj(m_simHandle, *this, std::move(updateData)));

    m_simHandle.PostReqCommand(new GSReqTxClientEvent(m_simHandle, *this, GSDefinitions::GSDefID_Freeze_LongLat, 1));
    m_simHandle.PostReqCommand(new GSReqTxClientEvent(m_simHandle, *this, GSDefinitions::GSDefID_Freeze_Altitude, 1));
    m_simHandle.PostReqCommand(new GSReqTxClientEvent(m_simHandle, *this, GSDefinitions::GSDefID_Freeze_Attitude, 1));
}
}
