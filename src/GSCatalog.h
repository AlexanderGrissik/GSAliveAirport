#pragma once

#include "GSSimConnect.h"
#include <random>
#include <cstddef>

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

    bool Exists(const char* str) const;
    const std::string& GetGPULargeExt() const { return m_extGPULarge; }
    const std::string& GetGPUMediumExt() const { return m_extGPUMedium; }
    const std::string& GetLavatoryExt() const { return m_extLavatory; }
    const std::string& GetTugExt();
    const std::string& GetBuggageLoaderExt();
    const std::string& GetBuggageWorkerExt();
    const std::string& GetTruckFacility();

private:
    GSCatalog() = default;
    ~GSCatalog() override {}
    GSCatalog(const GSCatalog &) = delete;
    GSCatalog &operator=(const GSCatalog &) = delete;

    using EntiresVec = std::vector<const std::pair<std::string, std::string>*>;
    std::stop_source m_source;
    std::vector<std::pair<std::string, std::string>> m_entires;
    EntiresVec m_entiresBuggageLoaderExt;
    EntiresVec m_entiresBuggageWorkerExt;
    EntiresVec m_entiresTugExt;
    EntiresVec m_entiresTruckFacility;
    std::string m_extGPULarge;
    std::string m_extGPUMedium;
    std::string m_extLavatory;
    std::string m_empty;
    std::mt19937 m_rng{std::random_device{}()};

    class GSCatalogReq : public GSRequest {
        using GSRequest::GSRequest;
        GSRequest::SendResult Process() override;
        bool OnMessage(SIMCONNECT_RECV* message, DWORD messageSize) override;
        void OnException(SIMCONNECT_RECV_EXCEPTION* message) override;
    };
};
} // namespace NS_GSLiveAirportMSFS
