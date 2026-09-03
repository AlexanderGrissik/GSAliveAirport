#include "GSReqCreateObject.h"

#include "GSReqCommand.h"
#include "../ISimConnectHandler.h"

#include <memory>

namespace parking_services
{
using namespace std::chrono_literals;

namespace
{
constexpr auto kRequestTimeout = 8s;
}

GSReqCreateObject::GSReqCreateObject(ISimConnectHandler *handler)
    : GSReqBase(kRequestTimeout), m_handler(handler)
{
}

void GSReqCreateObject::OnMessage(SIMCONNECT_RECV *message, DWORD)
{
    DWORD abandonedObjectId = 0;
    {
        std::scoped_lock lock(m_mutex);
        if (!PendingLocked()) return;
        if (!message || message->dwID != SIMCONNECT_RECV_ID_ASSIGNED_OBJECT_ID) {
            CompleteLocked(false);
            return;
        }
        const auto &assigned =
            *reinterpret_cast<SIMCONNECT_RECV_ASSIGNED_OBJECT_ID *>(message);
        m_objectId = assigned.dwObjectID;
        CompleteLocked(m_objectId != 0);
        if (m_abandoned && m_objectId != 0 && !m_removalIssued) {
            m_removalIssued = true;
            abandonedObjectId = m_objectId;
        }
    }
    RemoveAbandonedObject(abandonedObjectId);
}

DWORD GSReqCreateObject::ObjectId() const
{
    std::scoped_lock lock(m_mutex);
    ExpireLocked();
    return m_state == State::Succeeded ? m_objectId : 0;
}

void GSReqCreateObject::Abandon()
{
    DWORD abandonedObjectId = 0;
    {
        std::scoped_lock lock(m_mutex);
        m_abandoned = true;
        if (m_state == State::Succeeded && m_objectId != 0 && !m_removalIssued) {
            m_removalIssued = true;
            abandonedObjectId = m_objectId;
        }
    }
    RemoveAbandonedObject(abandonedObjectId);
}

void GSReqCreateObject::RemoveAbandonedObject(DWORD objectId)
{
    if (objectId == 0 || !m_handler) return;
    m_handler->RemoveObject(objectId, std::make_shared<GSReqCommand>());
}
} // namespace parking_services
