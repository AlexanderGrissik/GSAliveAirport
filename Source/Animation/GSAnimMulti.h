// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSAnimationObject.h"

#include <array>
#include <cstddef>
#include <vector>

namespace NS_GSAliveAirport
{

class GSAnimMulti : public GSAnimationObject
{
public:

    #pragma pack(push, 1)
    struct StateWireDataSet
    {
        float val;
    };
    #pragma pack(pop)

    GSAnimMulti(SIMCONNECT_OBJECT_ID objID, bool uncycle, GSDefinitions::GSDefID defID):
        GSAnimationObject(objID, uncycle), m_defID(defID) {}

    void Animate(GSSimConnect& handler, float elapsedMilli) override;
    void OnNextWayPoint(std::size_t idx) override;

    template <size_t N>
    void AddFrameSet(const std::array<FrameRangeInfo, N>& infoArr, bool order);

private:

    GSDefinitions::GSDefID m_defID;
    std::vector<GSAnimationObject::FrameRange> m_frameRanges;
    std::size_t m_activeFrameRange = 0;
};

template <size_t N>
void GSAnimMulti::AddFrameSet(const std::array<FrameRangeInfo, N>& infoArr, bool order)
{
    if (order) {
        for (const auto& info : infoArr)
            m_frameRanges.emplace_back(info);
    } else {
        for (auto it = infoArr.rbegin(); it != infoArr.rend(); ++it)
            m_frameRanges.emplace_back(*it);
    }
}

}
