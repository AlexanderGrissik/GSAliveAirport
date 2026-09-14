#include "GSCatalog.h"
#include "GSLogStream.h"
#include <regex>

namespace NS_GSLiveAirportMSFS
{

void GSCatalog::LoadCatalog()
{
    m_entires.clear();
    m_entires.reserve(1024);
    
    auto stop = m_source.get_token();
    while (!stop.stop_requested()) {
        RunDispatch(stop);
    }

    std::string strGPULargeExt = "FSDT_GPU_Hobart_4400_LW";
    std::string strGPUMediumExt = "FSDT_GPU_TLD_406_LW";
    std::string strLavatoryExt = "FSDT_Lavatory_Truck";
    for (const auto& pr : m_entires) {
        if (pr.first.starts_with(strGPULargeExt))
            m_extGPULarge = strGPULargeExt;
        else if (pr.first.starts_with(strGPUMediumExt))
            m_extGPUMedium = strGPUMediumExt;
        else if (pr.first.starts_with(strLavatoryExt))
            m_extLavatory = strLavatoryExt;
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