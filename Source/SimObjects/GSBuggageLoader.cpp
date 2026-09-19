#include "GSBuggageLoader.h"
#include "../General/GSGeography.h"
#include "../General/GSCatalog.h"
#include "GSStatic.h"
#include "GSBuggageTrain.h"
#include "../Animation/GSAnimBFLO.h"
#include "../Animation/GSAnimSingle.h"
#include <algorithm>

namespace NS_GSLiveAirportMSFS
{

constexpr std::array GSDatums_BuggageLoaderExtStateGet{
    GSDefinitions::DatumSpec{"BAGGAGELOADER END RAMP Y", "feet", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"BAGGAGELOADER END RAMP Z", "meters", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"BAGGAGELOADER PIVOT Y", "feet", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"BAGGAGELOADER PIVOT Z", "meters", SIMCONNECT_DATATYPE_FLOAT32},
};

constexpr std::array GSDatums_BuggageLoaderExtStateSet{
    GSDefinitions::DatumSpec{"BAGGAGELOADER ANGLE TARGET", "degrees", SIMCONNECT_DATATYPE_FLOAT32}
};

void GSBuggageLoader::InitDatums(GSSimConnect& handler)
{
    handler.InvokeAddDatums(GSDatums_BuggageLoaderExtStateGet, GSDefinitions::GSDefID::GSDefID_BuggageLoaderExtStateGet);
    handler.InvokeAddDatums(GSDatums_BuggageLoaderExtStateSet, GSDefinitions::GSDefID::GSDefID_BuggageLoaderExtStateSet);
}

bool GSBuggageLoader::OnCreated()
{
    m_simHandle.PostReqCommand(new GSReqGetDataSimObj(m_simHandle, *this));
    return false;
}

void GSBuggageLoader::OnArrived()
{
    SetFinalPositionAndStatePost();
}

void GSBuggageLoader::OnDespawning()
{
    m_simHandle.PostReqCommand(new GSReqTxEventEx1(
        m_simHandle, *this, GSDefinitions::GSDefID_CloseDoors,
        m_aircraft.GetObjID(), m_doorIdx + 1, 0));
}

bool GSBuggageLoader::PreSpawn()
{
    const auto& airData = m_aircraft.GetRawData();
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
    m_initPos.Heading = GSGeography::NormDeg(airData.headingDegrees + cargoDoor->first.get().headingDegrees + 180.0);
    m_initPos.Altitude = airData.groundAltitudeFeet;

    const auto doorPos = GSGeography::RelativePosition(airData.headingDegrees, m_aircraft.GetLongLat(), cargoDoor->first.get().posZMeter, cargoDoor->first.get().posXMeter);
    m_initPos.Longitude = doorPos.Long();
    m_initPos.Latitude = doorPos.Lat();

    m_doorPosElev = airData.alt_abv_grnd + cargoDoor->first.get().posYFeet; 

    PrepareRoute();

    return true;
}

void GSBuggageLoader::SetFinalPositionAndState(SIMCONNECT_RECV_SIMOBJECT_DATA& entry)
{
    StateWireDataGet rawStateGet;
    GSSimConnect::ReadMsgData(&rawStateGet, sizeof(rawStateGet), entry);

    std::array updateData{ StateWireDataSet{ CalcLoaderOpenAngle(rawStateGet) } };
    m_simHandle.PostReqCommand(new GSReqSetDataSimObj(m_simHandle, *this, std::move(updateData)));

    const auto loaderPos = GSGeography::RelativePosition(m_initPos.Heading, { m_initPos.Longitude, m_initPos.Latitude }, -rawStateGet.endRampZMeters - 0.5, 0.0);
    m_initPos.Longitude = loaderPos.Long();
    m_initPos.Latitude = loaderPos.Lat();

    FinalizeRoute();
}

void GSBuggageLoader::SetFinalPositionAndStatePost()
{
    m_simHandle.PostReqCommand(new GSReqTxEventEx1(m_simHandle, *this, GSDefinitions::GSDefID_OpenDoors, m_aircraft.GetObjID(), m_doorIdx + 1, 0));

    Freeze();

    if (m_ext) {
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
    m_attached.emplace_back(ptrTrain);
        
    ContinueSpawn();
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
