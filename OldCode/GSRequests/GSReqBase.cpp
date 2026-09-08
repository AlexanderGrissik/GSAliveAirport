#include "GSReqBase.h"

namespace parking_services
{
GSReqBase::GSReqBase(
    std::optional<std::chrono::steady_clock::duration> timeout)
{
    if (timeout) m_deadline = std::chrono::steady_clock::now() + *timeout;
}

void GSReqBase::OnFailure()
{
    std::scoped_lock lock(m_mutex);
    if (m_state != State::Pending) return;
    m_responseComplete = true;
    m_responseSucceeded = false;
    m_state = State::Failed;
}

void GSReqBase::OnSuccess()
{
    std::scoped_lock lock(m_mutex);
    if (m_state != State::Pending) return;
    ExpireLocked();
    if (!m_responseComplete) return;
    m_state = m_responseSucceeded ? State::Succeeded : State::Failed;
}

bool GSReqBase::IsComplete() const
{
    std::scoped_lock lock(m_mutex);
    ExpireLocked();
    return m_responseComplete;
}

bool GSReqBase::IsFinished() const
{
    std::scoped_lock lock(m_mutex);
    return m_state != State::Pending;
}

bool GSReqBase::Succeeded() const
{
    std::scoped_lock lock(m_mutex);
    return m_state == State::Succeeded;
}

bool GSReqBase::PendingLocked() const
{
    ExpireLocked();
    return !m_responseComplete;
}

void GSReqBase::CompleteLocked(bool succeeded)
{
    if (m_responseComplete) return;
    m_responseComplete = true;
    m_responseSucceeded = succeeded;
}

void GSReqBase::ExpireLocked() const
{
    if (!m_responseComplete && m_deadline &&
        std::chrono::steady_clock::now() >= *m_deadline) {
        m_responseComplete = true;
        m_responseSucceeded = false;
    }
}
} // namespace parking_services
