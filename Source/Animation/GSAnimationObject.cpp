#include "GSAnimationObject.h"
#include "../General/GSLogStream.h"

namespace NS_GSLiveAirportMSFS
{

void GSAnimationObject::InitDatums(GSSimConnect& handler)
{
    handler.InvokeAddDatums(GSDatums_AnimVelocY, GSDefinitions::GSDefID::GSDefID_AnimVelocBodyY);
    handler.InvokeAddDatums(GSDatums_AnimWagonBLO, GSDefinitions::GSDefID::GSDefID_AnimWagonBLO);
    handler.InvokeAddDatums(GSDatums_AnimWagonBFLO, GSDefinitions::GSDefID::GSDefID_AnimWagonBFLO);
    handler.InvokeAddDatums(GSDatums_AnimWagonBFLOT, GSDefinitions::GSDefID::GSDefID_AnimWagonBFLOT);
}

void GSAnimationObject::AddFrameSet(AnimKey key, float startFrame, float endFrame, float fps)
{
    GSDefinitions::GSDefID defID = GSDefinitions::GSDefID::GSDefID_AnimVelocBodyY;
    if (key == GSAnimationObject::WAGON_BLO)
        defID = GSDefinitions::GSDefID::GSDefID_AnimWagonBLO;
    else if (key == GSAnimationObject::WAGON_BLO)
        defID = GSDefinitions::GSDefID::GSDefID_AnimWagonBFLO;
    else if (key == GSAnimationObject::WAGON_BLO)
        defID = GSDefinitions::GSDefID::GSDefID_AnimWagonBFLOT;

    if (endFrame != startFrame)
        m_frames.emplace_back(
            defID, startFrame, endFrame, fps, startFrame, std::abs(endFrame - startFrame));
}

void GSAnimationObject::Animate(GSSimConnect& handler, float elapsedMilli)
{
    StateWireDataSet valSet;
    for (auto& fset : m_frames) {
        CalcNextFrame(fset, elapsedMilli);
        valSet.val = fset.m_currFrame;

        auto simRC = handler.Invoke(SimConnect_SetDataOnSimObject, fset.m_defID, m_simObjectID, 0, 1, static_cast<DWORD>(sizeof(valSet)), &valSet);
        if (!simRC.isOK()) {
            GSLogStream::LogError("GSAnimationObject::Animate Failed SimConnect_SetDataOnSimObject call: ") << simRC.rc;
        }
    }
}

void GSAnimationObject::CalcNextFrame(GSAnimationObject::FrameRange& frange, float elapsedMilli) 
{
    const float delta = frange.m_fps * elapsedMilli / 1000.0f;
    if (frange.m_endFrame > frange.m_startFrame) {
        frange.m_currFrame = frange.m_startFrame + std::fmod(frange.m_currFrame - frange.m_startFrame + delta, frange.m_span);
    } else {
        frange.m_currFrame = frange.m_startFrame - std::fmod(frange.m_startFrame - frange.m_currFrame + delta, frange.m_span);
    }
}

}
