#pragma once

#include "GSSimConnect.h"

namespace NS_GSLiveAirportMSFS
{

class GSAnimationObject
{
public:

    enum AnimKey {
        VELOCITY_BODY_Y = 0,
        WAGON_BLO,
        WAGON_FLO,
        END_KEY
    };

    #pragma pack(push, 1)
    struct StateWireDataSet
    {
        float val;
    };
    #pragma pack(pop)

    GSAnimationObject(SIMCONNECT_OBJECT_ID objID) : m_simObjectID(objID) {}

    static void InitDatums(GSSimConnect& handler);

    void Animate(GSSimConnect& handler, float elapsedMilli);
    void AddFrameSet(AnimKey key, float startFrame, float endFrame, float fps);
    void ResetObjID(SIMCONNECT_OBJECT_ID objID) { m_simObjectID = objID; }

    SIMCONNECT_OBJECT_ID GetObjID() const { return m_simObjectID; }

private:

    struct FrameRange {
        GSDefinitions::GSDefID m_defID;
        float m_startFrame;
        float m_endFrame;
        float m_fps;
        float m_currFrame;
        float m_span;
    };

    void CalcNextFrame(FrameRange& frange, float elapsedMilli);

    SIMCONNECT_OBJECT_ID m_simObjectID;
    std::vector<FrameRange> m_frames;
};

}

