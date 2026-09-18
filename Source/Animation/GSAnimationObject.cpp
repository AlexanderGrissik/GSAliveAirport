#include "GSAnimationObject.h"
#include "../General/GSLogStream.h"

namespace NS_GSLiveAirportMSFS
{

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
