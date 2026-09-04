#include "GSReqObjectPose.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace parking_services
{
using namespace std::chrono_literals;

namespace
{
constexpr auto kRequestTimeout = 8s;

#pragma pack(push, 1)
struct ObjectPoseWireData
{
    double latitude{};
    double longitude{};
    double altitudeFeet{};
    double headingDegrees{};
};
#pragma pack(pop)

static_assert(sizeof(ObjectPoseWireData) == 32);
}

GSReqObjectPose::GSReqObjectPose() : GSReqBase(kRequestTimeout) {}

void GSReqObjectPose::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    std::scoped_lock lock(m_mutex);
    if (!PendingLocked()) return;
    if (!message || message->dwID != SIMCONNECT_RECV_ID_SIMOBJECT_DATA) {
        CompleteLocked(false);
        return;
    }

    const auto &entry = *reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA *>(message);
    const auto *entryBytes = reinterpret_cast<const BYTE *>(&entry);
    const auto *payloadBytes = reinterpret_cast<const BYTE *>(&entry.dwData);
    const std::size_t offset = static_cast<std::size_t>(payloadBytes - entryBytes);
    const std::size_t received = entry.dwSize != 0
                                     ? (std::min)(static_cast<std::size_t>(entry.dwSize),
                                                  static_cast<std::size_t>(messageSize))
                                     : static_cast<std::size_t>(messageSize);
    if (received < offset + sizeof(ObjectPoseWireData)) {
        CompleteLocked(false);
        return;
    }

    ObjectPoseWireData pose{};
    std::memcpy(&pose, payloadBytes, sizeof(pose));
    if (!std::isfinite(pose.latitude) || !std::isfinite(pose.longitude) ||
        !std::isfinite(pose.altitudeFeet) ||
        !std::isfinite(pose.headingDegrees)) {
        CompleteLocked(false);
        return;
    }

    m_result = {true, pose.latitude, pose.longitude, pose.altitudeFeet,
                pose.headingDegrees};
    CompleteLocked(true);
}

GSObjectPoseResult GSReqObjectPose::Result() const
{
    std::scoped_lock lock(m_mutex);
    ExpireLocked();
    return m_state == State::Succeeded ? m_result : GSObjectPoseResult{};
}
} // namespace parking_services
