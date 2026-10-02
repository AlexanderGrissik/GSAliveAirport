// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSCmdReq.h"
#include <functional>

namespace NS_GSAliveAirport
{

class GSCmdReqIdent : public GSCmdReq
{
public:

    using Callable = std::function<void(SIMCONNECT_RECV_EXCEPTION *message)>;

    GSCmdReqIdent(int cmd, ReqPtr& req, Callable&& clb) : 
        GSCmdReq(cmd, req), m_callable(std::move(clb)) {}

    ~GSCmdReqIdent() override = default;

    Callable& GetCallable() { return m_callable; }

private:

    Callable m_callable;
};

} // namespace NS_GSAliveAirport
