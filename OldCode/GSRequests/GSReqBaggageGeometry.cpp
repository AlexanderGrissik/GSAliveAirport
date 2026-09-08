#include "GSReqBaggageGeometry.h"

#include <algorithm>
#include <cstring>
#include <optional>

namespace parking_services
{
using namespace std::chrono_literals;

namespace
{
constexpr auto kRequestTimeout = 8s;

#pragma pack(push, 1)
struct BaggageLoaderGeometryWireData
{
    double angleCurrentDegrees{};
    double endRampYMeters{};
    double endRampZMeters{};
    double pivotYMeters{};
    double pivotZMeters{};
};
#pragma pack(pop)

static_assert(sizeof(BaggageLoaderGeometryWireData) == 40);

std::optional<BaggageLoaderGeometryWireData> ReadPayload(
    const SIMCONNECT_RECV_SIMOBJECT_DATA &entry, DWORD messageSize)
{
    const auto *entryBytes = reinterpret_cast<const BYTE *>(&entry);
    const auto *payloadBytes = reinterpret_cast<const BYTE *>(&entry.dwData);
    const std::size_t offset = static_cast<std::size_t>(payloadBytes - entryBytes);
    const std::size_t received = entry.dwSize != 0
                                     ? (std::min)(static_cast<std::size_t>(entry.dwSize),
                                                  static_cast<std::size_t>(messageSize))
                                     : static_cast<std::size_t>(messageSize);
    if (received < offset + sizeof(BaggageLoaderGeometryWireData)) {
        return std::nullopt;
    }

    BaggageLoaderGeometryWireData payload{};
    std::memcpy(&payload, payloadBytes, sizeof(payload));
    return payload;
}
} // namespace

GSReqBaggageGeometry::GSReqBaggageGeometry() : GSReqBase(kRequestTimeout) {}

void GSReqBaggageGeometry::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    std::scoped_lock lock(m_mutex);
    if (!PendingLocked()) return;
    if (!message || message->dwID != SIMCONNECT_RECV_ID_SIMOBJECT_DATA) {
        CompleteLocked(false);
        return;
    }

    const auto &entry = *reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA *>(message);
    const auto payload = ReadPayload(entry, messageSize);
    if (!payload) {
        CompleteLocked(false);
        return;
    }
    m_result = {true, payload->angleCurrentDegrees, payload->endRampYMeters,
                payload->endRampZMeters, payload->pivotYMeters,
                payload->pivotZMeters};
    CompleteLocked(true);
}

GSBaggageGeometryResult GSReqBaggageGeometry::Result() const
{
    std::scoped_lock lock(m_mutex);
    ExpireLocked();
    return m_state == State::Succeeded ? m_result : GSBaggageGeometryResult{};
}
} // namespace parking_services
