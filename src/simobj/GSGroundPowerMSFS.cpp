#include "GSGroundPowerMSFS.h"
#include "../GSGeography.h"
#include "../GSCatalog.h"
#include "GSStatic.h"

namespace NS_GSLiveAirportMSFS
{

constexpr std::array GSDatums_GroundPowerMSFSStateSet {
    GSDefinitions::DatumSpec{"GROUNDPOWERUNIT HOSE DEPLOYED", "percent over 100", SIMCONNECT_DATATYPE_FLOAT32},
};

constexpr std::array GSDatums_GroundPowerExtStateSet {
    GSDefinitions::DatumSpec{"PLANE TOUCHDOWN BANK DEGREES", "degrees", SIMCONNECT_DATATYPE_FLOAT32},
};

void GSGroundPowerMSFS::InitDatums(GSSimConnect& handler)
{
    handler.InvokeAddDatums(GSDatums_GroundPowerMSFSStateSet, GSDefinitions::GSDefID::GSDefID_GroundPowerStateSet);
    handler.InvokeAddDatums(GSDatums_GroundPowerExtStateSet, GSDefinitions::GSDefID::GSDefID_GroundPowerExtStateSet);
}

void GSGroundPowerMSFS::OnCreated()
{
    SetFinalPositionAndState();
    SpawnAttached();
}

void GSGroundPowerMSFS::OnSpawned(bool ok, GSSimObj& obj)
{
    (void)obj; (void)ok;
    OnObjSpawned(true);
}

void GSGroundPowerMSFS::OnDespawned(bool ok, GSSimObj& obj)
{
    (void)obj; (void)ok;
    m_attached.clear();
    Despawn();
}

bool GSGroundPowerMSFS::PreSpawn()
{
    double YMeters = 0.0, XMeters = 0.0;
    const auto& airData = m_aircraft.GetRawData();
    auto groundPowerDoor = m_aircraft.GetGroundPowerDoor();
    auto frontLeftDoor = m_aircraft.GetFrontLeftDoor();
    if (groundPowerDoor.has_value()) {
        YMeters += groundPowerDoor->first.get().posZMeter;
        XMeters += groundPowerDoor->first.get().posXMeter;
    } else if (frontLeftDoor.has_value()) {
        YMeters += frontLeftDoor->first.get().posZMeter + 7;
        XMeters += frontLeftDoor->first.get().posXMeter - 4;
    } else {
        return false;
    }
    
    const auto* powerDoor = (groundPowerDoor.has_value() ? &groundPowerDoor->first.get() : (frontLeftDoor.has_value() ? &frontLeftDoor->first.get() : nullptr));
    if (!powerDoor) {
        return false;
    }

    
    bool driver = false;

    if (m_gpuType == GPU_LARGE && !GSCatalog::GetInstance().GetGPULargeExt().empty()) {
        m_title = GSCatalog::GetInstance().GetGPULargeExt();
        m_initPos.Heading = airData.headingDegrees;
        driver = true;
    } else if (m_gpuType == GPU_MEDIUM && !GSCatalog::GetInstance().GetGPUMediumExt().empty()) {
        m_title = GSCatalog::GetInstance().GetGPUMediumExt();
        m_initPos.Heading = airData.headingDegrees;
        driver = true;
    } else {
        m_title = "Car Ground Power Unit";
        m_initPos.Heading = GSGeography::NormDeg(airData.headingDegrees + 180.0);
    }
        
    m_initPos.Heading -= 45.0;

    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    m_initPos.OnGround = 1;
    m_initPos.Altitude = airData.groundAltitudeFeet;
    
    const auto doorPos = GSGeography::RelativePosition(
        airData.headingDegrees, m_aircraft.GetLongLat(), YMeters, XMeters);
    m_initPos.Longitude = doorPos.Long();
    m_initPos.Latitude = doorPos.Lat();

    if (driver) {
        const auto driverPos = GSGeography::RelativePosition(
            m_initPos.Heading, { m_initPos.Longitude, m_initPos.Latitude }, 1.54733, -0.35635);
        
        auto ptrDriver = new GSStatic(m_simHandle, m_aircraft, *this);
        ptrDriver->SetTitle("FSDT_Driver_01_Tug_M1A");
        ptrDriver->SetPosition({ driverPos.Long(), driverPos.Lat()});
        ptrDriver->SetPosRel(GSStatic::REL_ABSOLUTE);
        ptrDriver->SetHeading(m_initPos.Heading);
        ptrDriver->GetInitPos().Altitude = 1.0961 + m_initPos.Altitude;
        ptrDriver->GetInitPos().OnGround = 0;
        ptrDriver->PreSpawn();
        m_attached.emplace_back(ptrDriver);
    }

    return true;
}

void GSGroundPowerMSFS::SetFinalPositionAndState()
{
    std::array updateData1{ StateWireDataSet{ 1.0f } };
    m_simHandle.PostReqCommand(new GSReqSetDataSimObj(
        m_simHandle, *this, GSDefinitions::GSDefID::GSDefID_GroundPowerStateSet, std::move(updateData1)));

    std::array updateData2{ StateWireDataSet{ 1.0f } };
    m_simHandle.PostReqCommand(new GSReqSetDataSimObj(
        m_simHandle, *this, GSDefinitions::GSDefID::GSDefID_GroundPowerExtStateSet, std::move(updateData2)));

    Freeze();
}
}
