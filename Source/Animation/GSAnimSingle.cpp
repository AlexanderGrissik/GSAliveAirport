#include "GSAnimSingle.h"
#include "../General/GSLogStream.h"

namespace NS_GSLiveAirportMSFS
{

constexpr std::array GSDatums_AnimVelocY{
    GSDefinitions::DatumSpec{"VELOCITY BODY Y", "number", SIMCONNECT_DATATYPE_FLOAT32},
};

constexpr std::array GSDatums_AnimWagonBLO{
    GSDefinitions::DatumSpec{"WAGON BACK LINK ORIENTATION", "number", SIMCONNECT_DATATYPE_FLOAT32},
};


void GSAnimSingle::InitDatums(GSSimConnect& handler)
{
    handler.InvokeAddDatums(GSDatums_AnimVelocY, GSDefinitions::GSDefID::GSDefID_AnimVelocBodyY);
    handler.InvokeAddDatums(GSDatums_AnimWagonBLO, GSDefinitions::GSDefID::GSDefID_AnimWagonBLO);
}

void GSAnimSingle::AddFrameSet(GSDefinitions::GSDefID defID, float startFrame, float endFrame, float fps)
{
    m_frame = { startFrame, endFrame, fps };
    m_defID = defID;
}

void GSAnimSingle::Animate(GSSimConnect& handler, float elapsedMilli)
{
    CalcNextFrame(m_frame, elapsedMilli);
    
    StateWireDataSet valSet { m_frame.m_currFrame };
    auto simRC = handler.Invoke(
        SimConnect_SetDataOnSimObject, m_defID, m_simObjectID,
        0, 1, static_cast<DWORD>(sizeof(valSet)), &valSet);
    if (!simRC.isOK()) {
        GSLogStream::LogError("GSAnimSingle::Animate Failed SimConnect_SetDataOnSimObject call: ") << simRC.rc;
    }
}

}
