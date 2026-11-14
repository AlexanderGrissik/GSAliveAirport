// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSCommand.h"
#include "Requests/GSRequest.h"
#include <functional>
#include <utility>

namespace NS_GSAliveAirport
{

class GSCmdReq : public GSCommand
{
public:

    using Callable = std::function<void(SIMCONNECT_RECV_EXCEPTION* message)>;

    GSCmdReq(GSCommandID cmd, GSRequest* req): GSCommand(cmd), m_Req(req) {}
    GSCmdReq(GSCommandID cmd, GSRequest* req, std::shared_ptr<GSTimePoint>& execTime): GSCommand(cmd, execTime), m_Req(req) {}
    GSCmdReq(GSCommandID cmd, GSRequest* req, Callable&& exceptionCallback):
        GSCommand(cmd), m_Req(req), m_exceptionCallback(std::move(exceptionCallback)) {}
    virtual ~GSCmdReq() {}

    GSRequestU ReleaseReq() { return std::move(m_Req); }
    Callable& GetExceptionCallback() { return m_exceptionCallback; }

private:

    GSRequestU m_Req;
    Callable m_exceptionCallback;
};
} // namespace NS_GSAliveAirport
