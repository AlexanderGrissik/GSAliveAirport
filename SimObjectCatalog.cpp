#include "SimObjectCatalog.h"

#include "SimConnectIds.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <tuple>

namespace parking_services
{
SimObjectCatalog::SimObjectCatalog(LogSink log) : m_log(std::move(log)) {}

void SimObjectCatalog::Request(SimConnectSession &session, CompletionCallback callback)
{
    if (!session.IsConnected()) {
        std::cout << "Not connected to MSFS.\n";
        return;
    }
    if (m_inProgress) {
        if (callback) {
            m_callbacks.push_back(std::move(callback));
            return;
        }
        std::cout << "A SimObject catalog request is already in progress.\n";
        return;
    }

    m_buckets.clear();
    m_callbacks.clear();
    if (callback) m_callbacks.push_back(std::move(callback));
    const auto addBucket = [this, &session](std::string label, SIMCONNECT_SIMOBJECT_TYPE type,
                                            bool excluded) {
        const DWORD requestId = session.NextRequestId();
        if (label == "ALL") m_allRequestId = requestId;
        m_buckets.emplace(requestId, Bucket{std::move(label), type, excluded});
    };
    addBucket("ALL", SIMCONNECT_SIMOBJECT_TYPE_ALL, false);
    addBucket("AIRCRAFT", SIMCONNECT_SIMOBJECT_TYPE_AIRCRAFT, true);
    addBucket("HELICOPTER", SIMCONNECT_SIMOBJECT_TYPE_HELICOPTER, true);
    addBucket("BOAT", SIMCONNECT_SIMOBJECT_TYPE_BOAT, true);
    addBucket("GROUND", SIMCONNECT_SIMOBJECT_TYPE_GROUND, false);
    addBucket("HOT_AIR_BALLOON", SIMCONNECT_SIMOBJECT_TYPE_HOT_AIR_BALLOON, true);
    addBucket("ANIMAL", SIMCONNECT_SIMOBJECT_TYPE_ANIMAL, true);
    m_inProgress = true;
    for (auto &[requestId, bucket] : m_buckets) {
        const auto result = session.EnumerateObjects(requestId, bucket.type);
        if (!result.Succeeded()) {
            bucket.complete = true;
            std::cout << "MSFS rejected the " << bucket.label << " SimObject catalog request.\n";
        } else {
            session.TrackOperation(result, requestId, " while enumerating " + bucket.label,
                                   [this, requestId] {
                                       if (auto bucket = m_buckets.find(requestId);
                                           bucket != m_buckets.end()) {
                                           bucket->second.complete = true;
                                           FinishIfComplete();
                                       }
                                   });
        }
    }
    if (std::ranges::all_of(m_buckets, [](const auto &item) { return item.second.complete; })) {
        FinishIfComplete();
        return;
    }
    std::cout << "Requested categorized spawnable SimObject catalogs. Waiting for MSFS...\n";
}

void SimObjectCatalog::HandleData(
    SimConnectSession &session,
    const SIMCONNECT_RECV_ENUMERATE_SIMOBJECT_AND_LIVERY_LIST &message, DWORD messageSize)
{
    const auto bucketIt = m_buckets.find(message.dwRequestID);
    if (!m_inProgress || bucketIt == m_buckets.end()) {
        return;
    }
    const auto *messageBytes = reinterpret_cast<const BYTE *>(&message);
    const auto *entriesBytes = reinterpret_cast<const BYTE *>(&message.rgData);
    const std::size_t offset = static_cast<std::size_t>(entriesBytes - messageBytes);
    const std::size_t received = message.dwSize != 0
                                     ? (std::min)(static_cast<std::size_t>(message.dwSize),
                                                  static_cast<std::size_t>(messageSize))
                                     : static_cast<std::size_t>(messageSize);
    const std::size_t available = received > offset
                                      ? (received - offset) / sizeof(SIMCONNECT_ENUMERATE_SIMOBJECT_LIVERY)
                                      : 0;
    const std::size_t count = (std::min)(static_cast<std::size_t>(message.dwArraySize), available);
    for (std::size_t index = 0; index < count; ++index) {
        const auto &entry = message.rgData[index];
        bucketIt->second.entries.emplace(entry.AircraftTitle, entry.LiveryName);
    }
    if (count != message.dwArraySize) {
        m_log("The SimObject catalog contained a truncated response page.");
    }
    if (message.dwOutOf == 0 || message.dwEntryNumber + 1 >= message.dwOutOf) {
        bucketIt->second.complete = true;
        session.CompleteRequest(message.dwRequestID);
        FinishIfComplete();
    }
}

void SimObjectCatalog::Reset()
{
    m_buckets.clear();
    m_allRequestId.reset();
    m_callbacks.clear();
    m_inProgress = false;
}

bool SimObjectCatalog::InProgress() const
{
    return m_inProgress;
}

void SimObjectCatalog::FinishIfComplete()
{
    if (!m_inProgress ||
        !std::ranges::all_of(m_buckets, [](const auto &item) { return item.second.complete; })) {
        return;
    }
    m_inProgress = false;

    std::vector<std::string> availableTitles;
    std::vector<std::tuple<Entry, std::string>> retainedEntries;
    std::size_t excluded = 0;
    if (!m_allRequestId || !m_buckets.contains(*m_allRequestId)) {
        m_log("The all-objects catalog bucket was not available.");
    } else {
        const auto &allEntries = m_buckets.at(*m_allRequestId).entries;
        std::set<std::string> uniqueTitles;
        for (const auto &entry : allEntries) {
            bool isExcluded = false;
            std::string type = "UNKNOWN";
            for (const auto &[requestId, bucket] : m_buckets) {
                if (requestId == *m_allRequestId || !bucket.entries.contains(entry)) continue;
                if (bucket.excluded) {
                    isExcluded = true;
                    break;
                }
                if (type == "UNKNOWN") type = bucket.label;
            }
            if (isExcluded) {
                ++excluded;
            } else {
                retainedEntries.emplace_back(entry, std::move(type));
                uniqueTitles.insert(entry.first);
            }
        }
        availableTitles.assign(uniqueTitles.begin(), uniqueTitles.end());
    }

    std::array<wchar_t, 32768> executablePath{};
    const DWORD length = GetModuleFileNameW(nullptr, executablePath.data(),
                                            static_cast<DWORD>(executablePath.size()));
    std::filesystem::path outputPath = "simobject_catalog.txt";
    if (length != 0 && length < executablePath.size()) {
        outputPath = std::filesystem::path(executablePath.data()).parent_path() / outputPath;
    }
    std::ofstream output(outputPath, std::ios::trunc);
    if (!output) {
        m_log("Could not create SimObject catalog file: " + outputPath.string());
    } else {
        const std::size_t allCount = m_allRequestId && m_buckets.contains(*m_allRequestId)
                                         ? m_buckets.at(*m_allRequestId).entries.size()
                                         : 0;
        output << "ParkingServices filtered spawnable SimObject catalog\n"
               << "All entries: " << allCount << "\n"
               << "Excluded types: AIRCRAFT, HELICOPTER, HOT_AIR_BALLOON, BOAT, ANIMAL\n\n"
               << "[SPAWNABLE - FILTERED]\n";
        for (const auto &[entry, type] : retainedEntries) {
            output << "type=\"" << type << "\"\ttitle=\"" << entry.first
                   << "\"\tlivery=\"" << entry.second << "\"\n";
        }
        output << "\nRetained entries: " << retainedEntries.size()
               << "\nExcluded entries: " << excluded << "\n";
        m_log("Wrote SimObject catalog to " + outputPath.string());
    }

    auto callbacks = std::move(m_callbacks);
    m_callbacks.clear();
    for (auto &callback : callbacks) callback(availableTitles);
}
} // namespace parking_services
