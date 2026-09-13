#pragma once

#include "GSSimConnect.h"


namespace NS_GSLiveAirportMSFS
{

class GSCatalog final : public GSSimConnect
{
public:

    static GSCatalog &GetInstance() { static GSCatalog s_instance; return s_instance; }

    void LoadCatalog();

    void OnConnect();
    void OnDisconnect() override {}
    void OnSimStart() override {}
    void OnSimStop() override {}
    void OnCommand(GSCommand& cmd) override { (void)cmd; }
    void Done();

private:
    GSCatalog() = default;
    ~GSCatalog() override {}
    GSCatalog(const GSCatalog &) = delete;
    GSCatalog &operator=(const GSCatalog &) = delete;

    std::stop_source m_source;
    std::vector<std::pair<std::string, std::string>> m_entires;

    class GSCatalogReq : public GSRequest {
        using GSRequest::GSRequest;
        GSRequest::SendResult Process() override;
        bool OnMessage(SIMCONNECT_RECV* message, DWORD messageSize) override;
        void OnException(SIMCONNECT_RECV_EXCEPTION* message) override;
    };
};
} // namespace NS_GSLiveAirportMSFS
