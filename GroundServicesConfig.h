#pragma once

#include "Aircraft.h"

#include <filesystem>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace parking_services
{
struct GroundServiceAnimationCarrier
{
    std::string carrier;
    double firstFrame{};
    double lastFrame{};
};

struct GroundServiceAnimation
{
    double framesPerSecond{};
    bool reversible{};
    bool reversed{};
    std::vector<GroundServiceAnimationCarrier> carriers;
};

// A resolved node in a configured service tree. Every node has one selected
// spawnable SimObject title, its optional animation, and direct children.
struct GroundServiceObject
{
    std::string family;
    std::string title;
    std::optional<GroundServiceAnimation> animation;

    // Relative to the direct parent. X is parent-forward; negative Y is to the
    // parent's right, matching the XYZH convention in the configuration file.
    double parentX{};
    double parentY{};
    double parentZ{};
    double parentHeadingDegrees{};
    std::vector<GroundServiceObject> attachments;
};

enum class GroundServiceSpecialType
{
    None,
    LuggageLoaderFSDT,
    WalkerFSDT
};

enum class GroundServiceLocationKind
{
    Static,
    Route,
    // Attach to a right cargo door, preferring the back door and falling back to
    // the front door. Applied automatically to LuggageLoaderFSDT families.
    CargoDoorRightAuto
};

struct GroundServiceLocation
{
    GroundServiceLocationKind kind{GroundServiceLocationKind::Static};
    bool faceAircraft{};
    double relX1{};
    double relY1{};
    double relX2{};
    double relY2{};
};

struct GroundServiceRequest
{
    GroundServiceObject object;
    GroundServiceLocation location;
    // Carried through so the ground-service object factory can dispatch to the
    // right GSObject subclass (WalkerFSDT / LuggageLoaderFSDT / plain).
    GroundServiceSpecialType specialType{GroundServiceSpecialType::None};
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
    std::filesystem::path m_path;
    std::vector<std::string> m_startupMessages;
    std::map<std::string, Family, std::less<>> m_families;
    std::map<std::string, Location, std::less<>> m_locations;
    std::map<AircraftSizeCategory, std::vector<Element>> m_categories;
};
} // namespace parking_services
