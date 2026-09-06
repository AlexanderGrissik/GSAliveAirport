#include "SimConnectHandler.h"

void SimConnectHandler::ReadMsgData(void* dest, size_t destSize, const SIMCONNECT_RECV_SIMOBJECT_DATA& entry)
{
    const auto* begin = reinterpret_cast<const BYTE*>(&entry);
    const auto* data = reinterpret_cast<const BYTE*>(&entry.dwData);

    const size_t offset = data - begin;
    const size_t payloadSize = entry.dwSize - offset;

    std::memcpy(dest, data, std::min(payloadSize, destSize));
}