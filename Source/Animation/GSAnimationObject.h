#pragma once

#include "../General/GSDefinitions.h"
#include "../General/GSSimConnect.h"
#include "../General/GSCoord.h"

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

    static void InitDatums(GSSimConnect& handler);

    virtual void Animate(GSSimConnect& handler, float elapsedMilli) = 0;
    
    void ResetObjID(SIMCONNECT_OBJECT_ID objID) { m_simObjectID = objID; }
    SIMCONNECT_OBJECT_ID GetObjID() const { return m_simObjectID; }

    void SetAIWaypoints(const std::vector<SIMCONNECT_DATA_WAYPOINT>* aiWaypoints);

protected:

    static constexpr double ArrivalRadiusMeters = 0.10;

    void CalcNextFrame(FrameRange& frange, float elapsedMilli);
    void Move(GSSimConnect& handler, float elapsedMilli);

    SIMCONNECT_OBJECT_ID m_simObjectID;
    const std::vector<SIMCONNECT_DATA_WAYPOINT>* m_aiWaypoints;
    GSCoord m_currLocation;
    std::size_t m_tgtWaypoint = 1U;
};

}

