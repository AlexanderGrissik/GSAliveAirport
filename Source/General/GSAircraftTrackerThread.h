// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "../SimObjects/GSAircraft.h"
#include "../General/GSGeography.h"
#include "GSSimConnect.h"
#include "GSAirport.h"
#include "../Requests/GSReqTrackerEventAirportAdded.h"
#include "../Requests/GSReqTrackerEventAirportRemoved.h"
#include <chrono>
#include <memory>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace NS_GSAliveAirport
{

class GSAircraftTrackerThread final : public GSSimConnect
{
public:
    static constexpr DWORD DiscoveryRadiusMeters = 5000;
    static constexpr double DiscoveryRadiusDegPow = GSGeography::DegDistPowFromMeters(DiscoveryRadiusMeters);
    static constexpr double MinAirportRefreshDistMtr = 2000;
    static constexpr double MinAirportRefreshDistDegPow = GSGeography::DegDistPowFromMeters(MinAirportRefreshDistMtr);

    ~GSAircraftTrackerThread() override {}
    GSAircraftTrackerThread(GSSimConnect& singleObserver, GSSimConnect& animThread, GSAircraft& userAircraft):
        m_singleObserver(singleObserver), m_animThread(animThread), m_userAircraft(userAircraft),
        m_airportAddedReqID(NextRequestID()), m_airportRemovedReqID(NextRequestID()),
        m_airportAddedReq(*this), m_airportRemovedReq(*this) {}
    GSAircraftTrackerThread(const GSAircraftTrackerThread &) = delete;
    GSAircraftTrackerThread &operator=(const GSAircraftTrackerThread &) = delete;

    void Start();
    void Stop();
    
    void OnConnect() override;
    void OnDisconnect() override;
    void OnSimStart() override;
    void OnSimStop() override;
    void OnCommand(GSCommand& cmd) override;

    void SetInProgress(bool v) { m_scanInProgress = v; }
    void SetAirportScanInProgress(bool v) { m_scanAirportInProgress = v; }

    using AirportScanCache = std::unordered_map<std::string, GSCoord>;

    void HandleRemoved();
    void ClearFailedAircraft(DWORD objectID) { m_tracked.extract(objectID); }
    bool HandleScanMessage(const SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE& entry);
    void HandleAircraftConstScan(const SIMCONNECT_RECV_SIMOBJECT_DATA& entry, const AirportScanCache::value_type& airportInfo);
    bool HandleScanAirportMessage(const SIMCONNECT_RECV_AIRPORT_LIST &entry);
    void HandleScanAirportRemovedMessage(const SIMCONNECT_RECV_AIRPORT_LIST &entry);
    void HandleAirportInfoMessage(const SIMCONNECT_RECV_FACILITY_DATA& entry, DWORD aircraftID, const AirportScanCache::value_type& airportInfo);
    GSReqRC SpawnTestAircraft(int type, SIMCONNECT_DATA_INITPOSITION& pos);

  private:

    const char* GetDebugName() const override { return "GSAircraftTrackerThread"; }

    static void AircraftTrackerLoop(std::stop_token stopToken, GSAircraftTrackerThread *self);
    void RunLoopTracker(std::stop_token stopToken);

    void RequestScan();
    void RequestAirportScan();
    bool HandleExistingAircraft(GSAircraft& aircraft, std::shared_ptr<GSAircraft>& exisitng, const SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE& entry);
    void PostNewAircraft(std::shared_ptr<GSAircraft>& aircraft);
    void HandleAircraftUser(const SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry);
    void SpawnTestAircrafts();
    void SubscribeAirportList();
    void UnsubscribeAirportList();
    const AirportScanCache::value_type* FindClosesAirportToAircraft(const GSAircraft& aircraft) const;
    const AirportScanCache::value_type* IsNewAircraftEligable(const GSAircraft& aircraft) const;

    std::unordered_map<DWORD, std::shared_ptr<GSAircraft>> m_tracked;
    std::unordered_set<DWORD> m_lastScanIDs;
    GSInvokeRC m_rc{};
    bool m_scanInProgress = false;
    bool m_scanAirportInProgress = false;
    GSSimConnect& m_singleObserver;
    GSSimConnect& m_animThread;
    GSTimePoint m_lastScanTime{};
    GSTimePoint m_lastAirportScanTime{};
    std::jthread m_thread;
    GSAircraft& m_userAircraft;
    AirportScanCache m_airportCache;
    std::unordered_map<std::string, std::pair<size_t, std::shared_ptr<GSAirport>>> m_activeAirports;
    DWORD m_airportAddedReqID = 0;
    DWORD m_airportRemovedReqID = 0;

    GSReqTrackerEventAirportAdded m_airportAddedReq;
    GSReqTrackerEventAirportRemoved m_airportRemovedReq;
};
} // namespace NS_GSAliveAirport
