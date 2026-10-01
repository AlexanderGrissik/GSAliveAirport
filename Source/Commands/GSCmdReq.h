// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSCommand.h"
#include "../General/GSRequest.h"

namespace NS_GSLiveAirportMSFS
{

class GSCmdReq : public GSCommand
{
public:

    using ReqPtr = std::unique_ptr<GSRequest>;

    GSCmdReq(int cmd, ReqPtr& req): GSCommand(cmd), m_Req(std::move(req)) {}
    virtual ~GSCmdReq() {}

    ReqPtr ReleaseReq() { return std::move(m_Req); }

private:

    ReqPtr m_Req;
};
} // namespace NS_GSLiveAirportMSFS