#include "GSCateringCartMSFS.h"
#include "../GSGeography.h"

namespace NS_GSLiveAirportMSFS
{

constexpr std::array GSDatums_CateringTruckStateGet {
    GSDefinitions::DatumSpec{"CATERINGTRUCK AIRCRAFT DOOR CONTACT OFFSET Z", "meters", SIMCONNECT_DATATYPE_FLOAT32},
};

constexpr std::array GSDatums_CateringTruckStateSet {
    GSDefinitions::DatumSpec{"PLANE LONGITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64},
    GSDefinitions::DatumSpec{"PLANE LATITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64},
    GSDefinitions::DatumSpec{"CATERINGTRUCK ELEVATION TARGET", "meters", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"CATERINGTRUCK OPENING TARGET", "percent over 100", SIMCONNECT_DATATYPE_FLOAT32},
};

void GSCateringCartMSFS::InitDatums(GSSimConnect& handler)
{
    handler.InvokeAddDatums(GSDatums_CateringTruckStateGet, GSDefinitions::GSDefID::GSDefID_CateringTruckStateGet);
    handler.InvokeAddDatums(GSDatums_CateringTruckStateSet, GSDefinitions::GSDefID::GSDefID_CateringTruckStateSet);
}

void GSCateringCartMSFS::OnCreated()
{
    m_simHandle.PostReqCommand(new GSReqGetDataSimObj(m_simHandle, *this));
}

void GSCateringCartMSFS::PreSpawn()
{
//    m_doorPosElev =
//        (m_aircraft.altitudeFeet - m_aircraft.groundAltitudeFeet) * kFeetToMeters +
//        door->verticalMeters;
}

void GSCateringCartMSFS::SetFinalPositionAndState(SIMCONNECT_RECV_SIMOBJECT_DATA& entry)
{
    StateWireDataGet rawStateGet;
    GSSimConnect::ReadMsgData(&rawStateGet, sizeof(rawStateGet), entry);
    GSCoord out = GSGeography::RepositionZOffset({m_initPos.Longitude, m_initPos.Latitude}, m_initPos.Heading, -rawStateGet.doorContactOffsetZ);
    m_initPos.Longitude = out.Long();
    m_initPos.Latitude = out.Lat();

    std::array updateData{ StateWireDataSet{ m_initPos.Longitude, m_initPos.Latitude, m_doorPosElev, 1.0f } };
    m_simHandle.PostReqCommand(new GSReqSetDataSimObj(m_simHandle, *this, std::move(updateData)));

    m_simHandle.PostReqCommand(new GSReqTxClientEvent(m_simHandle, *this, GSDefinitions::GSDefID_Freeze_LongLat, 1));
    m_simHandle.PostReqCommand(new GSReqTxClientEvent(m_simHandle, *this, GSDefinitions::GSDefID_Freeze_Altitude, 1));
    m_simHandle.PostReqCommand(new GSReqTxClientEvent(m_simHandle, *this, GSDefinitions::GSDefID_Freeze_Attitude, 1));

    SetInProgress(false);
}
}
