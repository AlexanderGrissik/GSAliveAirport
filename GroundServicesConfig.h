#pragma once

#include "Aircraft.h"

#include <filesystem>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace parking_services
{
struct GroundServiceRequest
{
    std::string family;
    std::string title;
    bool walking{};
    bool faceAircraft{};
    double relX1{};
    double relY1{};
    double relX2{};
    double relY2{};
};

class GroundServicesConfig final
{
  public:
    static GroundServicesConfig LoadDefault();

    [[nodiscard]] bool IsLoaded() const;
    [[nodiscard]] bool IsResolved() const;
    [[nodiscard]] const std::filesystem::path &Path() const;
    [[nodiscard]] const std::vector<std::string> &StartupMessages() const;

    void Resolve(const std::vector<std::string> &availableTitles,
                 std::vector<std::string> &messages);
    void FillRequests(AircraftSizeCategory category, std::mt19937 &random,
                      std::vector<GroundServiceRequest> &destination) const;
    void ClearResolution();

  private:
    struct Family
    {
        std::string name;
        std::vector<std::string> preferredPatterns;
        std::vector<std::string> alternatePatterns;
        std::vector<std::string> excludePatterns;
        std::vector<std::string> resolvedTitles;
        bool usingAlternate{};
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
        std::string location;
    };

    static std::filesystem::path FindDefaultPath();
    void Load(const std::filesystem::path &path);

    bool m_loaded{};
    bool m_resolved{};
    std::filesystem::path m_path;
    std::vector<std::string> m_startupMessages;
    std::map<std::string, Family, std::less<>> m_families;
    std::map<std::string, Location, std::less<>> m_locations;
    std::map<AircraftSizeCategory, std::vector<Element>> m_categories;
};
} // namespace parking_services
