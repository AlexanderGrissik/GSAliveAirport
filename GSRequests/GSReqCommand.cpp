#include "GSReqCommand.h"

namespace parking_services
{
GSReqCommand::GSReqCommand() : GSReqBase(std::nullopt) {}

void GSReqCommand::OnMessage(SIMCONNECT_RECV *, DWORD)
{
    std::scoped_lock lock(m_mutex);
    CompleteLocked(true);
}
} // namespace parking_services
