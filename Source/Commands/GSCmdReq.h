// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSCommand.h"
#include "../Requests/GSRequest.h"

namespace NS_GSAliveAirport
{

class GSCmdReq : public GSCommand
{
public:

    GSCmdReq(GSCommandID cmd, GSRequest* req): GSCommand(cmd), m_Req(req) {}
    GSCmdReq(GSCommandID cmd, GSRequest* req, GSTimePoint execTime): GSCommand(cmd, execTime), m_Req(req) {}
    virtual ~GSCmdReq() {}

    GSRequestU ReleaseReq() { return std::move(m_Req); }

private:

    GSRequestU m_Req;
};
} // namespace NS_GSAliveAirport
