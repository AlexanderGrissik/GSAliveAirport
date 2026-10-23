// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "../General/GSDefinitions.h"
#include "../General/GSSimConnect.h"
#include "../General/GSCoord.h"

namespace NS_GSAliveAirport
{

class GSAnimationObject
{
public:

    struct FrameRangeInfo {
        float m_startFrame;
        float m_endFrame;
        float m_fps;

        float getSpan() const { return std::abs(m_endFrame - m_startFrame); }
    };

    GSAnimationObject(SIMCONNECT_OBJECT_ID objID) : m_simObjectID(objID) {}
    GSAnimationObject(SIMCONNECT_OBJECT_ID objID, bool uncycle) : m_simObjectID(objID), m_uncycle(uncycle) {}
    virtual ~GSAnimationObject() {}

    static void InitDatums(GSSimConnect& handler);

    virtual void Animate(GSSimConnect& handler, float elapsedMilli) = 0;
    virtual void OnNextWayPoint(std::size_t idx) = 0;
    
    void ResetObjID(SIMCONNECT_OBJECT_ID objID) { m_simObjectID = objID; }
    SIMCONNECT_OBJECT_ID GetObjID() const { return m_simObjectID; }

    void SetAIWaypoints(const std::vector<SIMCONNECT_DATA_WAYPOINT>* aiWaypoints);

protected:

    class FrameRange : public FrameRangeInfo {
    public:
        FrameRange() = default;
        FrameRange(const FrameRangeInfo& info): 
            FrameRangeInfo(info), m_currFrame(info.m_startFrame), m_span(info.getSpan()) {}
        
        FrameRange& operator=(const FrameRangeInfo& rg)
        {
            FrameRangeInfo::operator=(rg);
            m_currFrame = rg.m_startFrame;
            m_span = rg.getSpan();
            return *this;
        }

        float m_currFrame;
        float m_span;
    };

    static constexpr double ArrivalRadiusMeters = 0.10;

    void CalcNextFrame(FrameRange& frange, float elapsedMilli);
    void Move(GSSimConnect& handler, float elapsedMilli);

    SIMCONNECT_OBJECT_ID m_simObjectID;
    const std::vector<SIMCONNECT_DATA_WAYPOINT>* m_aiWaypoints = nullptr;
    GSCoord m_currLocation;
    float m_currAlt = 0.0f;
    std::size_t m_tgtWaypoint = 1U;
    bool m_uncycle = false;
};

}

