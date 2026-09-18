#pragma once

#include "GSAnimationObject.h"

namespace NS_GSLiveAirportMSFS
{

class GSAnimBFLOT : public GSAnimationObject
{
public:

    #pragma pack(push, 1)
    struct StateWireDataSet
    {
        float blo;
        float flo;
        float tdw;
    };
    #pragma pack(pop)

    GSAnimBFLOT(SIMCONNECT_OBJECT_ID objID) : GSAnimationObject(objID) {}

    static void InitDatums(GSSimConnect& handler);

    void Animate(GSSimConnect& handler, float elapsedMilli) override;
    void AddFrameSet(
        float startFrameB, float endFrameB, float fpsB, float startFrameF, float endFrameF, float fpsF,
        float startFrameT, float endFrameT, float fpsT);

private:

    GSAnimationObject::FrameRange m_BLO;
    GSAnimationObject::FrameRange m_FLO;
    GSAnimationObject::FrameRange m_TDW;
};

}

