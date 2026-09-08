#include "GSReqGroundScan.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <optional>
#include <utility>

namespace parking_services
{
using namespace std::chrono_literals;

namespace
{
constexpr auto kRequestTimeout = 8s;

template <std::size_t Size> std::string FixedString(const std::array<char, Size> &value)
{
    const auto end = std::find(value.begin(), value.end(), '\0');
    return {value.data(), static_cast<std::size_t>(end - value.begin())};
}

#pragma pack(push, 1)
struct GroundWireData
{
    std::array<char, 256> title{};
    double latitude{};
    double longitude{};
    double groundSpeedKnots{};
};
#pragma pack(pop)

static_assert(sizeof(GroundWireData) == 280);

std::optional<GroundWireData> ReadPayload(
    const SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry, DWORD messageSize)
{
    const auto *entryBytes = reinterpret_cast<const BYTE *>(&entry);
    const auto *payloadBytes = reinterpret_cast<const BYTE *>(&entry.dwData);
    const std::size_t offset = static_cast<std::size_t>(payloadBytes - entryBytes);
    const std::size_t received = entry.dwSize != 0
                                     ? (std::min)(static_cast<std::size_t>(entry.dwSize),
                                                  static_cast<std::size_t>(messageSize))
                                     : static_cast<std::size_t>(messageSize);
    if (received < offset + sizeof(GroundWireData)) return std::nullopt;

    GroundWireData payload{};
    std::memcpy(&payload, payloadBytes, sizeof(payload));
    return payload;
}
} // namespace

GSReqGroundScan::GSReqGroundScan() : GSReqBase(kRequestTimeout) {}

void GSReqGroundScan::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    std::scoped_lock lock(m_mutex);
    if (!PendingLocked()) return;
    if (!message || message->dwID != SIMCONNECT_RECV_ID_SIMOBJECT_DATA_BYTYPE) {
        CompleteLocked(false);
        return;
    }

    const auto &entry = *reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE *>(message);
    const auto payload = ReadPayload(entry, messageSize);
    if (!payload) {
        CompleteLocked(false);
        return;
    }
    if (entry.dwentrynumber == 1) m_batch.clear();
    m_batch[entry.dwObjectID] =
        {entry.dwObjectID, FixedString(payload->title), payload->latitude,
         payload->longitude, payload->groundSpeedKnots};
    if (entry.dwoutof != 0 && entry.dwentrynumber < entry.dwoutof) return;

    m_objects.clear();
    m_objects.reserve(m_batch.size());
    for (auto &[objectId, object] : m_batch) {
        static_cast<void>(objectId);
        m_objects.push_back(std::move(object));
    }
    CompleteLocked(true);
}

GSGroundScanResult GSReqGroundScan::TakeResult()
{
    std::scoped_lock lock(m_mutex);
    ExpireLocked();
    GSGroundScanResult result{m_state == State::Succeeded};
    if (result.succeeded) result.objects = std::move(m_objects);
    return result;
}
} // namespace parking_services
