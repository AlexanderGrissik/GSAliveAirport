// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "../Animation/GSAnimationObject.h"
#include "../General/GSSimConnect.h"
#include "GSCmdReq.h"

namespace NS_GSAliveAirport
{

class GSCmdAnimObj : public GSCommand
{
public:

    GSCmdAnimObj(int cmd, GSSimConnect& retSimConnect, GSAnimationObject* animObj):
        GSCommand(cmd), m_retSimConnect(retSimConnect), m_animObj(animObj) {
        m_objID = animObj->GetObjID();
    }
    GSCmdAnimObj(int cmd, GSSimConnect& retSimConnect, SIMCONNECT_OBJECT_ID animObj, GSRequest* retReq):
        GSCommand(cmd), m_retReq(retReq), m_retSimConnect(retSimConnect) {
        m_objID = animObj;
    }
    virtual ~GSCmdAnimObj() {}

    std::unique_ptr<GSAnimationObject>& AnimObj() { return m_animObj; }
    GSSimConnect& RetSimConn() const { return m_retSimConnect; }
    SIMCONNECT_OBJECT_ID ObjID() const { return m_objID; }
    GSRequest* ReleaseReq() { return m_retReq.release(); }
    GSRequest& GetReq() { return *m_retReq.get(); }

private:

    std::unique_ptr<GSAnimationObject> m_animObj;
    GSCmdReq::ReqPtr m_retReq;
    GSSimConnect& m_retSimConnect;
    SIMCONNECT_OBJECT_ID m_objID{};
};
} // namespace NS_GSAliveAirport
