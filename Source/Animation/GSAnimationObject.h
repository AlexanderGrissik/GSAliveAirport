#pragma once

#include "../General/GSDefinitions.h"
#include "../General/GSSimConnect.h"

namespace NS_GSLiveAirportMSFS
{

class GSAnimationObject
{
public:

    struct FrameRange {
        float m_startFrame;
        float m_endFrame;
        float m_fps;
        float m_currFrame;
        float m_span;
    };

    GSAnimationObject(SIMCONNECT_OBJECT_ID objID) : m_simObjectID(objID) {}
    virtual ~GSAnimationObject() {}

    virtual void Animate(GSSimConnect& handler, float elapsedMilli) = 0;
    
    void ResetObjID(SIMCONNECT_OBJECT_ID objID) { m_simObjectID = objID; }
    SIMCONNECT_OBJECT_ID GetObjID() const { return m_simObjectID; }

protected:

    void CalcNextFrame(FrameRange& frange, float elapsedMilli);

    SIMCONNECT_OBJECT_ID m_simObjectID;
};

}

