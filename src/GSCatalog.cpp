#include "GSCatalog.h"
#include "GSLogStream.h"

namespace NS_GSLiveAirportMSFS
{

void GSCatalog::LoadCatalog()
{
    auto stop = m_source.get_token();
    while (!stop.stop_requested()) {
        RunDispatch(stop);
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

GSRequest::SendResult GSCatalog::GSCatalogReq::Process()
{
    const DWORD requestId = m_simHandle.NextRequestID();
    GSRequest::SendResult rc = {
        m_simHandle.Invoke(SimConnect_EnumerateSimObjectsAndLiveries, requestId, SIMCONNECT_SIMOBJECT_TYPE_ALL), true };
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


    //for (std::size_t index = 0; index < count; ++index) {
    //    m_entires.emplace_back({ list->rgData[index].AircraftTitle, list->rgData[index].LiveryName });
    // }

    if (entry.dwOutOf == entry.dwEntryNumber) {
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