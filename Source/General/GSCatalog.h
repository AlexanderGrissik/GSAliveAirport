#pragma once

#include "GSSimConnect.h"
#include <cstddef>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

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
    const std::string& GetTugExt() { return GetEntry(m_entiresTarmac); }
    const std::string& GetBuggageLoaderExt() { return GetEntry(m_entiresBuggageLoaderExt); }
    const std::string& GetBuggageWorkerExt() { return GetEntry(m_entiresBuggageWorkerExt); }
    const std::string& GetPilotExt() { return GetEntry(m_entriesPilotExt); }
    const std::string& GetPassengerExt() { return GetEntry(m_entriesPassengerExt); }
    const std::string& GetTruckFacility() { return GetEntry(m_entiresTruckFacility); }
    const std::string& GetTarmacHuman() { return GetEntry(m_entiresTarmac); }
    const std::string& GetMarshallerExt() { return GetEntry(m_entriesMarshallerExt); }
    const std::string& GetMarshaller() { return GetEntry(m_entriesMarshaller); }
    const std::string& GetWingwalker() { return GetEntry(m_entriesWingwalkerExt); }

private:
    GSCatalog() = default;
    ~GSCatalog() override {}
    GSCatalog(const GSCatalog &) = delete;
    GSCatalog &operator=(const GSCatalog &) = delete;

    const char* GetDebugName() const override { return "GSCatalog"; }

    using EntiresVec = std::vector<const std::pair<std::string, std::string>*>;
    const std::string& GetEntry(EntiresVec& entires);

    std::stop_source m_source;
    std::vector<std::pair<std::string, std::string>> m_entires;
    EntiresVec m_entiresBuggageLoaderExt;
    EntiresVec m_entiresBuggageWorkerExt;
    EntiresVec m_entiresTugExt;
    EntiresVec m_entiresTruckFacility;
    EntiresVec m_entiresTarmac;
    EntiresVec m_entriesPilotExt;
    EntiresVec m_entriesPassengerExt;
    EntiresVec m_entriesMarshallerExt;
    EntiresVec m_entriesMarshaller;
    EntiresVec m_entriesWingwalkerExt;
    std::string m_extGPULarge;
    std::string m_extGPUMedium;
    std::string m_extLavatory;
    std::string m_empty;
    class GSCatalogReq : public GSRequest {
        using GSRequest::GSRequest;
        GSRequest::SendResult Process() override;
        bool OnMessage(SIMCONNECT_RECV* message, DWORD messageSize) override;
        void OnException(SIMCONNECT_RECV_EXCEPTION* message) override;
    };
};
} // namespace NS_GSLiveAirportMSFS
