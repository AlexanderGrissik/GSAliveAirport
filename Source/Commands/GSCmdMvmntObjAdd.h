// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSCmdSimObj.h"

namespace NS_GSAliveAirport
{

class GSCmdMvmntObjAdd : public GSCmdSimObj
{
public:

    GSCmdMvmntObjAdd(GSSimObj& simObj, bool reverseArrival) :
        GSCmdSimObj(GS_CMD_MVMNT_OBJ_ADD, simObj), m_reverseArrival(reverseArrival) {}

    bool ReverseArrival() const { return m_reverseArrival; }

private:

    bool m_reverseArrival;
};

} // namespace NS_GSAliveAirport
