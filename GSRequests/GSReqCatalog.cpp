#include "GSReqCatalog.h"

#include <algorithm>

namespace parking_services
{
void GSReqCatalog::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    std::scoped_lock lock(m_mutex);
    if (m_responseComplete) return;
    if (!message ||
        message->dwID != SIMCONNECT_RECV_ID_ENUMERATE_SIMOBJECT_AND_LIVERY_LIST) {
        m_entries.clear();
        m_responseComplete = true;
        m_responseSucceeded = false;
        return;
    }

    const auto *list =
        reinterpret_cast<const SIMCONNECT_RECV_ENUMERATE_SIMOBJECT_AND_LIVERY_LIST *>(
            message);
    const auto *messageBytes = reinterpret_cast<const BYTE *>(list);
    const auto *entriesBytes = reinterpret_cast<const BYTE *>(&list->rgData);
    const std::size_t offset =
        static_cast<std::size_t>(entriesBytes - messageBytes);
    const std::size_t received =
        list->dwSize != 0
            ? (std::min)(static_cast<std::size_t>(list->dwSize),
                         static_cast<std::size_t>(messageSize))
            : static_cast<std::size_t>(messageSize);
    const std::size_t available =
        received > offset
            ? (received - offset) / sizeof(SIMCONNECT_ENUMERATE_SIMOBJECT_LIVERY)
            : 0;
    const std::size_t count =
        (std::min)(static_cast<std::size_t>(list->dwArraySize), available);
    if (count != list->dwArraySize) {
        m_entries.clear();
        m_responseComplete = true;
        m_responseSucceeded = false;
        return;
    }
    for (std::size_t index = 0; index < count; ++index) {
        m_entries.push_back(
            {list->rgData[index].AircraftTitle, list->rgData[index].LiveryName});
    }

    if (list->dwOutOf == 0 || list->dwEntryNumber + 1 >= list->dwOutOf) {
        m_responseComplete = true;
        m_responseSucceeded = true;
    }
}

void GSReqCatalog::OnSuccess()
{
    std::scoped_lock lock(m_mutex);
    if (m_state != State::Pending || !m_responseComplete) return;
    if (!m_responseSucceeded) m_entries.clear();
    Finish(m_responseSucceeded ? State::Succeeded : State::Failed);
}

void GSReqCatalog::OnFailure()
{
    std::scoped_lock lock(m_mutex);
    if (m_state != State::Pending) return;
    m_entries.clear();
    m_responseComplete = true;
    m_responseSucceeded = false;
    Finish(State::Failed);
}

bool GSReqCatalog::IsComplete() const
{
    std::scoped_lock lock(m_mutex);
    return m_responseComplete;
}

void GSReqCatalog::Wait()
{
    std::unique_lock lock(m_mutex);
    m_completed.wait(lock, [this] { return m_state != State::Pending; });
}

bool GSReqCatalog::Succeeded() const
{
    std::scoped_lock lock(m_mutex);
    return m_state == State::Succeeded;
}

void GSReqCatalog::Finish(State state)
{
    m_state = state;
    m_completed.notify_all();
}
} // namespace parking_services
