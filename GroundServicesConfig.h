#pragma once

#include "Aircraft.h"
#include "GroundServiceTypes.h"
#include "SimObjectCatalog.h"

#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace parking_services
{
class ISimConnectHandler;

class GroundServicesConfig final
{
  public:
    static GroundServicesConfig LoadDefault();

    // Non-copyable (holds a mutex). Movable so the owning thread (GroundServicesThread)
    // can take ownership of a freshly loaded config. The move locks both objects' state
    // mutexes and transfers every field; the source is left valid and empty.
    GroundServicesConfig(GroundServicesConfig &&other);
    GroundServicesConfig(const GroundServicesConfig &) = delete;
    GroundServicesConfig &operator=(const GroundServicesConfig &) = delete;
    GroundServicesConfig &operator=(GroundServicesConfig &&) = delete;

    [[nodiscard]] bool IsLoaded() const;
    [[nodiscard]] bool IsResolved() const;
    [[nodiscard]] const std::filesystem::path &Path() const;
    [[nodiscard]] const std::vector<std::string> &StartupMessages() const;
    [[nodiscard]] const std::vector<std::string> &InitializationMessages() const;

    // Fetches the live SimObject catalog (via a GSReqCatalog executed on the
    // SimConnect thread) and resolves the configured service families against it.
    // Blocks the caller until the whole catalog has been loaded and resolved;
    // only SimConnectThread touches the session. Called from the GS thread's
    // connect job.
    void LoadCatalog(ISimConnectHandler &handler);
    void ResetInitialization();
    void Resolve(const std::vector<std::string> &availableTitles,
                 std::vector<std::string> &messages);
    void FillRequests(AircraftSizeCategory category, std::mt19937 &random,
                      std::vector<GroundServiceRequest> &destination) const;
    void ClearResolution();

  private:
    explicit GroundServicesConfig(std::filesystem::path path);

    struct RelativeTransform
    {
        double x{};
        double y{};
        double z{};
        double headingDegrees{};
    };

    struct Attachment
    {
        std::string family;
        std::vector<RelativeTransform> transforms;
        std::vector<Attachment> attachments;
    };

    struct Family
    {
        std::string name;
        std::vector<std::string> preferredPatterns;
        std::vector<std::string> alternatePatterns;
        std::vector<std::string> excludePatterns;
        std::string alternateFamily;
        GroundServiceSpecialType specialType{GroundServiceSpecialType::None};
        std::optional<GroundServiceAnimation> animation;
        std::vector<Attachment> attachments;
        std::vector<std::string> resolvedTitles;
        bool usingAlternateObjects{};
    };

    struct Location
    {
        std::string name;
        double randomOffsetX{};
        double randomOffsetY{};
        bool walking{};
        bool faceAircraft{};
        double relX1{};
        double relY1{};
        double relX2{};
        double relY2{};
    };

    struct Element
    {
        std::string family;
        std::vector<std::string> locations;
        unsigned int reverseProbabilityPercent{};
    };

    static std::filesystem::path FindDefaultPath();
    void Load(const std::filesystem::path &path);
    [[nodiscard]] bool BuildObject(std::string_view familyName,
                                   bool requestReverseAnimation,
                                   std::mt19937 &random,
                                   std::vector<std::string> &ancestry,
                                   GroundServiceObject &destination,
                                   bool &usedAlternateFamily) const;
    void BuildAttachments(const std::vector<Attachment> &attachments,
                          std::mt19937 &random,
                          std::vector<std::string> &ancestry,
                          std::vector<GroundServiceObject> &destination) const;
    [[nodiscard]] GroundServiceLocation SelectLocation(
        std::string_view name, std::mt19937 &random) const;

    bool m_loaded{};
    bool m_resolved{};
    mutable std::recursive_mutex m_stateMutex;
    std::filesystem::path m_path;
    std::vector<std::string> m_startupMessages;
    std::vector<std::string> m_initializationMessages;
    SimObjectCatalog m_catalog;
    std::map<std::string, Family, std::less<>> m_families;
    std::map<std::string, Location, std::less<>> m_locations;
    std::map<AircraftSizeCategory, std::vector<Element>> m_categories;
};
} // namespace parking_services
