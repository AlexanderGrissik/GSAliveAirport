#pragma once

#include "GSAnimationObject.h"

namespace NS_GSLiveAirportMSFS
{

class GSAnimSingle : public GSAnimationObject
{
public:

    #pragma pack(push, 1)
    struct StateWireDataSet
    {
        float val;
    };
    #pragma pack(pop)

    GSAGSAnimSinglenimBFLO(SIMCONNECT_OBJECT_ID objID) : GSAnimationObject(objID) {}

    static void InitDatums(GSSimConnect& handler);

    void Animate(GSSimConnect& handler, float elapsedMilli) override;
    void AddFrameSet(GSDefinitions::GSDefID defID, float startFrame, float endFrame, float fps);

private:

    GSAnimationObject::FrameRange m_frame;
    GSDefinitions::GSDefID m_defID;
};

}

