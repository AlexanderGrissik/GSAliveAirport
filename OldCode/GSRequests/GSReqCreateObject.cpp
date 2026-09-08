#include "GSReqCreateObject.h"

namespace parking_services
{
using namespace std::chrono_literals;

namespace
{
constexpr auto kRequestTimeout = 8s;
}

GSReqCreateObject::GSReqCreateObject() : GSReqBase(kRequestTimeout) {}

void GSReqCreateObject::OnMessage(SIMCONNECT_RECV *message, DWORD)
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
}

DWORD GSReqCreateObject::ObjectId() const
{
    std::scoped_lock lock(m_mutex);
    ExpireLocked();
    return m_state == State::Succeeded ? m_objectId : 0;
}
} // namespace parking_services
