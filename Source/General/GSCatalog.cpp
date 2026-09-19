#include "GSCatalog.h"
#include "GSLogStream.h"
#include "GSRandom.h"
#include <random>
#include <regex>

namespace NS_GSLiveAirportMSFS
{

void GSCatalog::LoadCatalog()
{
    m_entires.clear();
    m_entires.reserve(1024);
    m_entiresBuggageLoaderExt.clear();
    m_entiresBuggageLoaderExt.reserve(64);
    
    auto stop = m_source.get_token();
    while (!stop.stop_requested()) {
        RunDispatch(stop);
    }

    std::string strGPULargeExt = "FSDT_GPU_Hobart_4400_LW";
    std::string strGPUMediumExt = "FSDT_GPU_TLD_406_LW";
    std::string strLavatoryExt = "FSDT_Lavatory_Truck";
    std::string strBuggageLoaderExt = "FSDT_Tug_660";
    std::string strBuggageCargo = "CARGO";
    std::string strBuggageWorkerExt = "FSDT_Baggage_Loader_Man_02";
    std::string strTugExt = "FSDT_Tug_M1A";
    std::string strTruckFacility = "ASO_TruckFacility01";
    std::string strPilotExt = "FSDT_Passenger_PILOT";
    std::string strPassangerExt = "FSDT_Passenger_";
    std::string strSeatedExt = "SEATED";
    std::string strGuidnessExt = "guidness";
    std::string strTarmac = "Tarmac";
    std::string strMarshallerExt = "FSDT_Marshaller_";
    std::string strMarshallerMale = "Marshaller_Male";
    std::string strMarshallerFemale = "Marshaller_Female";
    std::string strWingwalkerExt = "FSDT_Wingwalker";
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
    }

    Disconnect();
}

void GSCatalog::OnConnect()
{
    PostReqCommand(new GSCatalogReq(*this));
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

    std::uniform_int_distribution<size_t> dist(0, entires.size() - 1);

    return entires[dist(GSRandom::Engine())]->first;
}

GSRequest::SendResult GSCatalog::GSCatalogReq::Process()
{
    const DWORD requestId = m_simHandle.NextRequestID();
    GSRequest::SendResult rc = {
        m_simHandle.InvokeRequest(requestId, SimConnect_EnumerateSimObjectsAndLiveries, requestId, SIMCONNECT_SIMOBJECT_TYPE_ALL), true };
    if (!rc.m_simRC.isOK()) {
        GSLogStream::LogError("GSCatalog::GSCatalogReq::Process Failed call: ") << rc.m_simRC.rc;
        rc.m_keep = false;
        static_cast<GSCatalog&>(m_simHandle).Done();
    }

    return rc;
}

bool GSCatalog::GSCatalogReq::OnMessage(SIMCONNECT_RECV* message, DWORD messageSize)
{
    (void)messageSize;
    const auto& entry = *reinterpret_cast<const SIMCONNECT_RECV_ENUMERATE_SIMOBJECT_AND_LIVERY_LIST*>(message);
    if (entry.dwOutOf == 0 || message->dwID != SIMCONNECT_RECV_ID_ENUMERATE_SIMOBJECT_AND_LIVERY_LIST) { // Empty Scan
        static_cast<GSCatalog&>(m_simHandle).Done();
        GSLogStream::LogError("GSCatalog::GSCatalogReq::OnMessage Empty Catalog. ID: ") << message->dwID << ", Count: " << entry.dwOutOf;
        return true;
    }

    auto &catalog = static_cast<GSCatalog&>(m_simHandle);
    for (DWORD index = 0; index < entry.dwArraySize; ++index) {
        catalog.m_entires.emplace_back(entry.rgData[index].AircraftTitle, entry.rgData[index].LiveryName);
    }

    if (entry.dwOutOf == entry.dwEntryNumber + 1) {
        static_cast<GSCatalog&>(m_simHandle).Done();
        return true;
    }

    return false;
}

void GSCatalog::GSCatalogReq::OnException(SIMCONNECT_RECV_EXCEPTION* message)
{
    static_cast<GSCatalog&>(m_simHandle).Done();
    GSLogStream::LogError("GSCatalog::GSCatalogReq::OnException Catalog Error. ID: ") << message->dwSendID << ", Err: " << message->dwException;
}



}
