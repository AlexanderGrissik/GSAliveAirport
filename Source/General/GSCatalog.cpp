// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSCatalog.h"
#include "Requests/GSReqCatalog.h"
#include "GSLogStream.h"
#include "GSRandom.h"
#include <algorithm>
#include <random>
#include <regex>

namespace NS_GSAliveAirport
{

void GSCatalog::LoadCatalog()
{
    static const std::string strGPULargeExt = "FSDT_GPU_Hobart_4400_LW";
    static const std::string strGPUMediumExt = "FSDT_GPU_TLD_406_LW";
    static const std::string strLavatoryExt = "FSDT_Lavatory_Truck";
    static const std::string strBuggageLoaderExt = "FSDT_Tug_660";
    static const std::string strBuggageCargo = "CARGO";
    static const std::string strBuggageWorkerExt = "FSDT_Baggage_Loader_Man_02";
    static const std::string strStaircaseExt = "FSDT_Staircase_TLD_ABS-580";
    static const std::string strTugExt = "FSDT_Tug_M1A";
    static const std::string strTruckFacility = "ASO_TruckFacility01";
    static const std::string strPilotExt = "FSDT_Passenger_PILOT";
    static const std::string strPassangerExt = "FSDT_Passenger_";
    static const std::string strSeatedExt = "SEATED";
    static const std::string strGuidnessExt = "guidness";
    static const std::string strTarmac = "Tarmac";
    static const std::string strMarshallerExt = "FSDT_Marshaller_";
    static const std::string strMarshallerMale = "Marshaller_Male";
    static const std::string strMarshallerFemale = "Marshaller_Female";
    static const std::string strWingwalkerExt = "FSDT_Wingwalker";

    static const auto strUtilityVehicle = std::to_array<std::string>({
        "Truck Water",
        "Truck Fuel",
        "Truck Utility",
        "Pickup",
        "ASO_TruckFacility",
        "ASO_Operation_Truck",
        "ASO_CarUtility01",
        "SUV_NorthAm"
    });

    m_entiresBuggageLoaderExt.clear();
    m_entiresBuggageLoaderExt.reserve(64);
    m_entiresBuggageWorkerExt.clear();
    m_entiresStaircaseExt.clear();
    m_entiresStaircaseExt.reserve(64);
    m_entiresTugExt.clear();
    m_entiresTruckFacility.clear();
    m_entiresTarmac.clear();
    m_entriesPilotExt.clear();
    m_entriesPassengerExt.clear();
    m_entriesMarshallerExt.clear();
    m_entriesMarshaller.clear();
    m_entriesWingwalkerExt.clear();
    m_entriesUtilityVehicle.clear();
    m_entriesWaterTruck.clear();
    m_entriesFuelTruck.clear();
    m_entriesUtilityTruck.clear();
    m_entriesPickup.clear();
    m_entriesUtilityTruckFacility.clear();
    m_entriesOperationTruck.clear();
    m_extGPULarge.clear();
    m_extGPUMedium.clear();
    m_extLavatory.clear();
    m_entires.clear();
    m_entires.reserve(1024);
    
    m_source = std::stop_source{};
    auto stop = m_source.get_token();
    while (!stop.stop_requested()) {
        RunDispatch(stop);
    }

    for (const auto& pr : m_entires) {
        if (pr.first.starts_with(strGPULargeExt))
            m_extGPULarge = strGPULargeExt;
        else if (pr.first.starts_with(strGPUMediumExt))
            m_extGPUMedium = strGPUMediumExt;
        else if (pr.first.starts_with(strLavatoryExt))
            m_extLavatory = strLavatoryExt;
        else if (pr.first.starts_with(strPilotExt))
            m_entriesPilotExt.push_back(&pr);
        else if (pr.first.starts_with(strWingwalkerExt))
            m_entriesWingwalkerExt.push_back(&pr);
        else if (pr.first.starts_with(strMarshallerExt))
            m_entriesMarshallerExt.push_back(&pr);
        else if (pr.first.starts_with(strMarshallerMale) || pr.first.starts_with(strMarshallerFemale))
            m_entriesMarshaller.push_back(&pr);
        else if (pr.first.starts_with(strPassangerExt) && (pr.first.find(strSeatedExt) == std::string::npos) && (pr.first.find(strGuidnessExt) == std::string::npos) && (pr.first.find(strPilotExt) == std::string::npos))
            m_entriesPassengerExt.push_back(&pr);
        else if (pr.first.starts_with(strTugExt))
            m_entiresTugExt.push_back(&pr);
        else if (pr.first.starts_with(strTruckFacility))
            m_entiresTruckFacility.push_back(&pr);
        else if (pr.first.starts_with(strTarmac))
            m_entiresTarmac.push_back(&pr);
        else if (pr.first.starts_with(strBuggageLoaderExt) && (pr.first.find(strBuggageCargo) == std::string::npos))
            m_entiresBuggageLoaderExt.push_back(&pr);
        else if (pr.first.starts_with(strBuggageWorkerExt) && (pr.first.find(strBuggageCargo) == std::string::npos))
            m_entiresBuggageWorkerExt.push_back(&pr);
        else if (pr.first.starts_with(strStaircaseExt))
            m_entiresStaircaseExt.push_back(&pr);
        if (std::any_of(strUtilityVehicle.begin(), strUtilityVehicle.end(), 
            [&pr](const auto& prefix) { return pr.first.starts_with(prefix); })) {
            m_entriesUtilityVehicle.push_back(&pr);

            if (pr.first.starts_with("Truck Water"))
                m_entriesWaterTruck.push_back(&pr);
            else if (pr.first.starts_with("Truck Fuel"))
                m_entriesFuelTruck.push_back(&pr);
            else if (pr.first.starts_with("Truck Utility"))
                m_entriesUtilityTruck.push_back(&pr);
            else if (pr.first.starts_with("Pickup"))
                m_entriesPickup.push_back(&pr);
            else if (pr.first.starts_with("ASO_TruckFacility"))
                m_entriesUtilityTruckFacility.push_back(&pr);
            else if (pr.first.starts_with("ASO_Operation_Truck"))
                m_entriesOperationTruck.push_back(&pr);
        }
    }

    Disconnect();
}

void GSCatalog::OnConnect()
{
    PostReqCommand(new GSReqCatalog(*this));
}

void GSCatalog::Done()
{
    m_source.request_stop();
}

bool GSCatalog::Exists(const char* str) const
{
    std::string temp = str;
    for (const auto& pr : m_entires) {
        if (pr.first == temp)
            return true;
    }

    return false;
}

const std::string& GSCatalog::GetEntry(EntiresVec& entires)
{
    if (entires.empty())
        return m_empty;

    return entires[GSRandom::RandSizeT(0, entires.size() - 1)]->first;
}


}
