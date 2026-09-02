#include "GroundServicesConfig.h"

#include <Windows.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace parking_services
{
namespace
{
using Json = nlohmann::json;
constexpr char kConfigurationFileName[] = "category-json-template.json";

std::string Lower(std::string_view value)
{
    std::string result(value);
    std::ranges::transform(result, result.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

bool EqualIgnoreCase(std::string_view left, std::string_view right)
{
    return Lower(left) == Lower(right);
}

bool WildcardMatch(std::string_view pattern, std::string_view value)
{
    const std::string lowerPattern = Lower(pattern);
    const std::string lowerValue = Lower(value);
    std::size_t patternIndex = 0;
    std::size_t valueIndex = 0;
    std::size_t wildcardIndex = std::string::npos;
    std::size_t wildcardValueIndex = 0;
    while (valueIndex < lowerValue.size()) {
        if (patternIndex < lowerPattern.size() &&
            lowerPattern[patternIndex] == lowerValue[valueIndex]) {
            ++patternIndex;
            ++valueIndex;
        } else if (patternIndex < lowerPattern.size() && lowerPattern[patternIndex] == '*') {
            wildcardIndex = patternIndex++;
            wildcardValueIndex = valueIndex;
        } else if (wildcardIndex != std::string::npos) {
            patternIndex = wildcardIndex + 1;
            valueIndex = ++wildcardValueIndex;
        } else {
            return false;
        }
    }
    while (patternIndex < lowerPattern.size() && lowerPattern[patternIndex] == '*') {
        ++patternIndex;
    }
    return patternIndex == lowerPattern.size();
}

std::vector<std::string> ReadStringArray(const Json &parent, std::string_view key,
                                         bool required)
{
    const auto item = parent.find(std::string(key));
    if (item == parent.end()) {
        if (required) throw std::runtime_error("missing required '" + std::string(key) + "'");
        return {};
    }
    if (!item->is_array()) {
        throw std::runtime_error("'" + std::string(key) + "' must be an array");
    }
    std::vector<std::string> values;
    values.reserve(item->size());
    for (const Json &value : *item) {
        if (!value.is_string() || value.get_ref<const std::string &>().empty()) {
            throw std::runtime_error("'" + std::string(key) +
                                     "' must contain non-empty strings");
        }
        values.push_back(value.get<std::string>());
    }
    return values;
}

std::string ReadName(const Json &value, std::string_view key)
{
    const std::string keyName(key);
    if (!value.contains(keyName) || !value.at(keyName).is_string() ||
        value.at(keyName).get_ref<const std::string &>().empty()) {
        throw std::runtime_error("missing or invalid '" + std::string(key) + "'");
    }
    return value.at(keyName).get<std::string>();
}

double ReadNumber(const Json &value, std::string_view key)
{
    const std::string keyName(key);
    if (!value.contains(keyName) || !value.at(keyName).is_number()) {
        throw std::runtime_error("missing or invalid numeric '" + std::string(key) + "'");
    }
    const double result = value.at(keyName).get<double>();
    if (!std::isfinite(result)) {
        throw std::runtime_error("'" + std::string(key) + "' must be finite");
    }
    return result;
}

AircraftSizeCategory ParseCategory(std::string_view name)
{
    const std::string normalized = Lower(name);
    if (normalized == "small") return AircraftSizeCategory::Small;
    if (normalized == "medium") return AircraftSizeCategory::Medium;
    if (normalized == "large") return AircraftSizeCategory::Large;
    if (normalized == "extralarge") return AircraftSizeCategory::ExtraLarge;
    throw std::runtime_error("unknown aircraft category '" + std::string(name) + "'");
}

std::vector<std::string> ExpandPatterns(const std::vector<std::string> &patterns,
                                        const std::vector<std::string> &availableTitles,
                                        const std::unordered_set<std::string> &excluded)
{
    std::vector<std::string> result;
    std::unordered_set<std::string> included;
    for (const std::string &pattern : patterns) {
        for (const std::string &title : availableTitles) {
            const std::string normalized = Lower(title);
            if (!excluded.contains(normalized) && WildcardMatch(pattern, title) &&
                included.insert(normalized).second) {
                result.push_back(title);
            }
        }
    }
    return result;
}

GroundServiceSpecialType ReadSpecialType(const Json &value, std::string_view owner)
{
    if (!value.contains("SpecialType")) {
        return GroundServiceSpecialType::None;
    }
    if (!value.at("SpecialType").is_string()) {
        throw std::runtime_error("SpecialType for '" + std::string(owner) +
                                 "' must be a string");
    }
    const std::string name = value.at("SpecialType").get<std::string>();
    if (EqualIgnoreCase(name, "LuggageLoaderFSDT")) {
        return GroundServiceSpecialType::LuggageLoaderFSDT;
    }
    if (EqualIgnoreCase(name, "WalkerFSDT")) {
        return GroundServiceSpecialType::WalkerFSDT;
    }
    throw std::runtime_error("unknown SpecialType '" + name + "' for '" +
                             std::string(owner) + "'");
}

GroundServiceAnimation ReadAnimation(const Json &value, std::string_view owner)
{
    if (!value.is_object()) {
        throw std::runtime_error("Animation for '" + std::string(owner) + "' must be an object");
    }
    GroundServiceAnimation animation{};
    animation.framesPerSecond = ReadNumber(value, "FPS");
    if (animation.framesPerSecond <= 0.0) {
        throw std::runtime_error("Animation FPS must be greater than zero");
    }
    if (const auto reversible = value.find("Reversable"); reversible != value.end()) {
        if (!reversible->is_boolean()) {
            throw std::runtime_error("Animation Reversable must be boolean");
        }
        animation.reversible = reversible->get<bool>();
    }
    const auto carriers = value.find("Carriers");
    if (carriers == value.end() || !carriers->is_array() || carriers->empty()) {
        throw std::runtime_error("Animation Carriers must be a non-empty array");
    }
    animation.carriers.reserve(carriers->size());
    for (const Json &carrierValue : *carriers) {
        if (!carrierValue.is_object()) {
            throw std::runtime_error("each Animation Carrier must be an object");
        }
        GroundServiceAnimationCarrier carrier{};
        carrier.carrier = ReadName(carrierValue, "Carrier");
        const auto frames = carrierValue.find("Frames");
        if (frames == carrierValue.end() || !frames->is_array() || frames->size() != 2 ||
            !(*frames)[0].is_number() || !(*frames)[1].is_number()) {
            throw std::runtime_error("Animation Carrier Frames must contain exactly two numbers");
        }
        carrier.firstFrame = (*frames)[0].get<double>();
        carrier.lastFrame = (*frames)[1].get<double>();
        if (!std::isfinite(carrier.firstFrame) || !std::isfinite(carrier.lastFrame)) {
            throw std::runtime_error("Animation Carrier Frames must be finite");
        }
        animation.carriers.push_back(std::move(carrier));
    }
    return animation;
}
} // namespace

GroundServicesConfig GroundServicesConfig::LoadDefault()
{
    GroundServicesConfig configuration;
    configuration.Load(FindDefaultPath());
    return configuration;
}

bool GroundServicesConfig::IsLoaded() const
{
    return m_loaded;
}

bool GroundServicesConfig::IsResolved() const
{
    return m_resolved;
}

const std::filesystem::path &GroundServicesConfig::Path() const
{
    return m_path;
}

const std::vector<std::string> &GroundServicesConfig::StartupMessages() const
{
    return m_startupMessages;
}

void GroundServicesConfig::Resolve(const std::vector<std::string> &availableTitles,
                                   std::vector<std::string> &messages)
{
    messages.clear();
    if (!m_loaded) return;

    std::vector<std::string> uniqueTitles;
    uniqueTitles.reserve(availableTitles.size());
    std::unordered_set<std::string> seen;
    for (const std::string &title : availableTitles) {
        if (!title.empty() && seen.insert(Lower(title)).second) uniqueTitles.push_back(title);
    }
    std::ranges::sort(uniqueTitles, [](const std::string &left, const std::string &right) {
        return Lower(left) < Lower(right);
    });

    for (auto &[name, family] : m_families) {
        std::unordered_set<std::string> excluded;
        for (const std::string &pattern : family.excludePatterns) {
            for (const std::string &title : uniqueTitles) {
                if (WildcardMatch(pattern, title)) excluded.insert(Lower(title));
            }
        }
        auto preferred = ExpandPatterns(family.preferredPatterns, uniqueTitles, excluded);
        auto alternate = ExpandPatterns(family.alternatePatterns, uniqueTitles, excluded);
        family.usingAlternateObjects = preferred.empty() && !alternate.empty();
        family.resolvedTitles = preferred.empty() ? std::move(alternate) : std::move(preferred);
        if (family.resolvedTitles.empty()) {
            messages.push_back("Family '" + name + "' resolved to no installed SimObjects" +
                               (family.alternateFamily.empty()
                                    ? "."
                                    : "; it will use AlternateFamily '" +
                                          family.alternateFamily + "' when requested."));
        } else {
            messages.push_back("Family '" + name + "' resolved to " +
                               std::to_string(family.resolvedTitles.size()) +
                               (family.usingAlternateObjects ? " alternate" : " preferred") +
                               " SimObject(s).");
        }
    }
    m_resolved = true;
}

void GroundServicesConfig::FillRequests(
    AircraftSizeCategory category, std::mt19937 &random,
    std::vector<GroundServiceRequest> &destination) const
{
    destination.clear();
    if (!m_loaded || !m_resolved) return;
    const auto categoryIt = m_categories.find(category);
    if (categoryIt == m_categories.end()) return;
    if (destination.capacity() < categoryIt->second.size()) {
        destination.reserve(categoryIt->second.size());
    }

    std::vector<std::string> ancestry;
    for (const Element &element : categoryIt->second) {
        GroundServiceRequest request{};
        ancestry.clear();
        std::bernoulli_distribution reverseSelection(
            static_cast<double>(element.reverseProbabilityPercent) / 100.0);
        bool usedAlternateFamily = false;
        if (!BuildObject(element.family, reverseSelection(random), random, ancestry,
                         request.object, usedAlternateFamily)) {
            continue;
        }
        const GroundServiceSpecialType specialType =
            m_families.contains(element.family)
                ? m_families.at(element.family).specialType
                : GroundServiceSpecialType::None;
        if (specialType == GroundServiceSpecialType::LuggageLoaderFSDT) {
            // Attach to a cargo door: prefer the back door, fall back to the front.
            request.location =
                GroundServiceLocation{GroundServiceLocationKind::CargoDoorRightAuto};
        } else {
            std::uniform_int_distribution<std::size_t> locationIndex(
                0, element.locations.size() - 1);
            request.location =
                SelectLocation(element.locations[locationIndex(random)], random);
        }
        destination.push_back(std::move(request));
    }
}

void GroundServicesConfig::ClearResolution()
{
    for (auto &[name, family] : m_families) {
        family.resolvedTitles.clear();
        family.usingAlternateObjects = false;
    }
    m_resolved = false;
}

std::filesystem::path GroundServicesConfig::FindDefaultPath()
{
    std::vector<std::filesystem::path> roots;
    std::error_code error;
    roots.push_back(std::filesystem::current_path(error));

    std::array<wchar_t, 32768> executablePath{};
    const DWORD length = GetModuleFileNameW(nullptr, executablePath.data(),
                                            static_cast<DWORD>(executablePath.size()));
    if (length != 0 && length < executablePath.size()) {
        auto root = std::filesystem::path(executablePath.data()).parent_path();
        while (!root.empty()) {
            roots.push_back(root);
            const auto parent = root.parent_path();
            if (parent == root) break;
            root = parent;
        }
    }
    for (const auto &root : roots) {
        const auto candidate = root / kConfigurationFileName;
        error.clear();
        if (std::filesystem::is_regular_file(candidate, error)) return candidate;
    }
    return roots.front() / kConfigurationFileName;
}

bool GroundServicesConfig::BuildObject(std::string_view familyName,
                                       bool requestReverseAnimation,
                                       std::mt19937 &random,
                                       std::vector<std::string> &ancestry,
                                       GroundServiceObject &destination,
                                       bool &usedAlternateFamily) const
{
    const auto family = m_families.find(familyName);
    if (family == m_families.end()) return false;
    const std::string normalizedName = Lower(familyName);
    if (std::ranges::find(ancestry, normalizedName) != ancestry.end()) return false;
    ancestry.push_back(normalizedName);

    if (family->second.resolvedTitles.empty()) {
        if (family->second.alternateFamily.empty()) {
            ancestry.pop_back();
            return false;
        }
        usedAlternateFamily = true;
        const bool built = BuildObject(family->second.alternateFamily,
                                       requestReverseAnimation, random, ancestry,
                                       destination, usedAlternateFamily);
        ancestry.pop_back();
        return built;
    }

    const Family &definition = family->second;
    std::uniform_int_distribution<std::size_t> titleIndex(
        0, definition.resolvedTitles.size() - 1);
    destination = {};
    destination.family = definition.name;
    destination.title = definition.resolvedTitles[titleIndex(random)];
    if (definition.animation) {
        destination.animation = *definition.animation;
        destination.animation->reversed =
            requestReverseAnimation && destination.animation->reversible;
    }
    BuildAttachments(definition.attachments, random, ancestry, destination.attachments);
    ancestry.pop_back();
    return true;
}

void GroundServicesConfig::BuildAttachments(
    const std::vector<Attachment> &attachments, std::mt19937 &random,
    std::vector<std::string> &ancestry,
    std::vector<GroundServiceObject> &destination) const
{
    for (const Attachment &attachment : attachments) {
        GroundServiceObject prototype{};
        bool usedAlternateFamily = false;
        if (!BuildObject(attachment.family, false, random, ancestry, prototype,
                         usedAlternateFamily)) {
            continue;
        }
        // A multi-coordinate child is configuration shorthand: select one
        // family member and clone that exact selected object at every XYZH.
        if (!usedAlternateFamily) {
            BuildAttachments(attachment.attachments, random, ancestry,
                             prototype.attachments);
        }
        for (const RelativeTransform &transform : attachment.transforms) {
            GroundServiceObject child = prototype;
            child.parentX = transform.x;
            child.parentY = transform.y;
            child.parentZ = transform.z;
            child.parentHeadingDegrees = transform.headingDegrees;
            destination.push_back(std::move(child));
        }
    }
}

GroundServiceLocation GroundServicesConfig::SelectLocation(
    std::string_view name, std::mt19937 &random) const
{
    const auto location = m_locations.find(name);
    if (location == m_locations.end()) return {};
    const Location &source = location->second;
    std::uniform_real_distribution<double> offsetX(-source.randomOffsetX,
                                                   source.randomOffsetX);
    std::uniform_real_distribution<double> offsetY(-source.randomOffsetY,
                                                   source.randomOffsetY);
    const double randomX = offsetX(random);
    const double randomY = offsetY(random);
    return {source.walking ? GroundServiceLocationKind::Route
                           : GroundServiceLocationKind::Static,
            source.faceAircraft,
            source.relX1 + randomX, source.relY1 + randomY,
            source.relX2 + randomX, source.relY2 + randomY};
}

void GroundServicesConfig::Load(const std::filesystem::path &path)
{
    m_path = path;
    try {
        std::ifstream input(path);
        if (!input) throw std::runtime_error("could not open the configuration file");
        const Json document = Json::parse(input);
        if (!document.is_object()) throw std::runtime_error("root value must be an object");

        const auto readAttachments = [&](const auto &self, const Json &parent,
                                         std::vector<Attachment> &destination) -> void {
            const auto entries = parent.find("AttachObjects");
            if (entries == parent.end()) return;
            if (!entries->is_array()) {
                throw std::runtime_error("AttachObjects must be an array");
            }
            destination.reserve(destination.size() + entries->size());
            for (const Json &entry : *entries) {
                if (!entry.is_object()) {
                    throw std::runtime_error("each AttachObjects entry must be an object");
                }
                Attachment attachment{};
                attachment.family = ReadName(entry, "Family");
                const auto transforms = entry.find("XYZH");
                if (transforms == entry.end() || !transforms->is_array() ||
                    transforms->empty()) {
                    throw std::runtime_error("AttachObjects XYZH must be a non-empty array");
                }
                attachment.transforms.reserve(transforms->size());
                for (const Json &transform : *transforms) {
                    if (!transform.is_array() || transform.size() != 4 ||
                        !transform[0].is_number() || !transform[1].is_number() ||
                        !transform[2].is_number() || !transform[3].is_number()) {
                        throw std::runtime_error(
                            "each AttachObjects XYZH entry must contain four numbers");
                    }
                    const RelativeTransform result{transform[0].get<double>(),
                                                   transform[1].get<double>(),
                                                   transform[2].get<double>(),
                                                   transform[3].get<double>()};
                    if (!std::isfinite(result.x) || !std::isfinite(result.y) ||
                        !std::isfinite(result.z) || !std::isfinite(result.headingDegrees)) {
                        throw std::runtime_error("AttachObjects XYZH values must be finite");
                    }
                    attachment.transforms.push_back(result);
                }
                self(self, entry, attachment.attachments);
                destination.push_back(std::move(attachment));
            }
        };

        const Json &families = document.at("Families");
        if (!families.is_array()) throw std::runtime_error("'Families' must be an array");
        for (const Json &value : families) {
            if (!value.is_object()) throw std::runtime_error("each family must be an object");
            Family family{};
            family.name = ReadName(value, "Family");
            family.preferredPatterns = ReadStringArray(value, "PreferedObj", true);
            if (family.preferredPatterns.empty()) {
                throw std::runtime_error("PreferedObj must not be empty");
            }
            family.alternatePatterns = ReadStringArray(value, "AlternateObj", false);
            family.excludePatterns = ReadStringArray(value, "Exclude", false);
            if (const auto alternate = value.find("AlternateFamily"); alternate != value.end()) {
                if (!alternate->is_string() || alternate->get_ref<const std::string &>().empty()) {
                    throw std::runtime_error("AlternateFamily must be a non-empty string");
                }
                family.alternateFamily = alternate->get<std::string>();
            }
            family.specialType = ReadSpecialType(value, family.name);
            if (const auto animation = value.find("Animation"); animation != value.end()) {
                family.animation = ReadAnimation(*animation, family.name);
            }
            readAttachments(readAttachments, value, family.attachments);
            if (!m_families.emplace(family.name, std::move(family)).second) {
                throw std::runtime_error("duplicate family name");
            }
        }

        const auto validateAttachments = [&](const auto &self,
                                             const std::vector<Attachment> &attachments) -> void {
            for (const Attachment &attachment : attachments) {
                if (!m_families.contains(attachment.family)) {
                    throw std::runtime_error("AttachObjects references unknown family '" +
                                             attachment.family + "'");
                }
                self(self, attachment.attachments);
            }
        };
        for (const auto &[name, family] : m_families) {
            if (!family.alternateFamily.empty() &&
                !m_families.contains(family.alternateFamily)) {
                throw std::runtime_error("Family '" + name +
                                         "' references unknown AlternateFamily '" +
                                         family.alternateFamily + "'");
            }
            validateAttachments(validateAttachments, family.attachments);
        }

        const Json &locations = document.at("Locations");
        if (!locations.is_array()) throw std::runtime_error("'Locations' must be an array");
        for (const Json &value : locations) {
            if (!value.is_object()) throw std::runtime_error("each location must be an object");
            Location location{};
            location.name = ReadName(value, "Name");
            if (value.contains("RandOffset")) {
                throw std::runtime_error("location '" + location.name +
                                         "' uses obsolete RandOffset; use RandOffsetX and RandOffsetY");
            }
            const bool hasRandomOffsetX = value.contains("RandOffsetX");
            const bool hasRandomOffsetY = value.contains("RandOffsetY");
            if (hasRandomOffsetX != hasRandomOffsetY) {
                throw std::runtime_error("location '" + location.name +
                                         "' must define both RandOffsetX and RandOffsetY");
            }
            if (hasRandomOffsetX) {
                location.randomOffsetX = ReadNumber(value, "RandOffsetX");
                location.randomOffsetY = ReadNumber(value, "RandOffsetY");
                if (location.randomOffsetX < 0.0 || location.randomOffsetY < 0.0) {
                    throw std::runtime_error("location random offsets cannot be negative");
                }
            }
            if (const auto direction = value.find("Dir"); direction != value.end()) {
                if (!direction->is_string() ||
                    Lower(direction->get_ref<const std::string &>()) != "aircraft") {
                    throw std::runtime_error("location '" + location.name +
                                             "' has unsupported Dir; expected 'Aircraft'");
                }
                location.faceAircraft = true;
            }
            const bool hasStatic = value.contains("RelX") || value.contains("RelY");
            const bool hasRoute = value.contains("RelX1") || value.contains("RelY1") ||
                                  value.contains("RelX2") || value.contains("RelY2");
            if (hasStatic == hasRoute) {
                throw std::runtime_error(
                    "location must define either RelX/RelY or both route endpoints");
            }
            location.walking = hasRoute;
            if (hasRoute) {
                location.relX1 = ReadNumber(value, "RelX1");
                location.relY1 = ReadNumber(value, "RelY1");
                location.relX2 = ReadNumber(value, "RelX2");
                location.relY2 = ReadNumber(value, "RelY2");
            } else {
                location.relX1 = location.relX2 = ReadNumber(value, "RelX");
                location.relY1 = location.relY2 = ReadNumber(value, "RelY");
            }
            if (!m_locations.emplace(location.name, std::move(location)).second) {
                throw std::runtime_error("duplicate location name");
            }
        }

        const Json &categories = document.at("Categories");
        if (!categories.is_array()) throw std::runtime_error("'Categories' must be an array");
        for (const Json &value : categories) {
            if (!value.is_object()) throw std::runtime_error("each category must be an object");
            const std::string categoryName = ReadName(value, "Category");
            const AircraftSizeCategory category = ParseCategory(categoryName);
            if (m_categories.contains(category)) {
                throw std::runtime_error("duplicate category '" + categoryName + "'");
            }
            std::vector<Element> elements;
            if (const auto item = value.find("Elements"); item != value.end()) {
                if (!item->is_array()) throw std::runtime_error("'Elements' must be an array");
                elements.reserve(item->size());
                for (const Json &entry : *item) {
                    if (!entry.is_object()) {
                        throw std::runtime_error("each category element must be an object");
                    }
                    Element element{};
                    element.family = ReadName(entry, "Family");
                    if (!m_families.contains(element.family)) {
                        throw std::runtime_error("category references unknown family '" +
                                                 element.family + "'");
                    }
                    const bool hasLocation = entry.contains("Location");
                    const bool hasLocations = entry.contains("Locations");
                    // A LuggageLoaderFSDT family attaches itself to a cargo door, so the
                    // category element does not need to provide a Location for it.
                    const bool suppliesOwnPlacement =
                        m_families.contains(element.family) &&
                        m_families.at(element.family).specialType ==
                            GroundServiceSpecialType::LuggageLoaderFSDT;
                    if (!suppliesOwnPlacement && hasLocation == hasLocations) {
                        throw std::runtime_error(
                            "category element must define exactly one of Location or Locations");
                    }
                    if (hasLocation) {
                        element.locations.push_back(ReadName(entry, "Location"));
                    } else if (hasLocations) {
                        element.locations = ReadStringArray(entry, "Locations", true);
                    }
                    if (!suppliesOwnPlacement && element.locations.empty()) {
                        throw std::runtime_error("category element Locations must not be empty");
                    }
                    for (const std::string &locationName : element.locations) {
                        if (!m_locations.contains(locationName)) {
                            throw std::runtime_error("category references unknown location '" +
                                                     locationName + "'");
                        }
                    }
                    if (const auto probability = entry.find("AnimReversePrb");
                        probability != entry.end()) {
                        if (!probability->is_number_unsigned() ||
                            probability->get<unsigned int>() > 100) {
                            throw std::runtime_error(
                                "AnimReversePrb must be an integer from 0 to 100");
                        }
                        element.reverseProbabilityPercent = probability->get<unsigned int>();
                    }
                    elements.push_back(std::move(element));
                }
            }
            m_categories.emplace(category, std::move(elements));
        }
        m_loaded = true;
        m_startupMessages.push_back("Loaded ground-service configuration from " + path.string());
    } catch (const std::exception &exception) {
        m_loaded = false;
        m_startupMessages.push_back("Ground-service configuration error in " + path.string() +
                                    ": " + exception.what());
    }
}
} // namespace parking_services
