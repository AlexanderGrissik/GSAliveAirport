#include "GroundServicesConfig.h"

#include <Windows.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <unordered_set>

namespace parking_services
{
namespace
{
using Json = nlohmann::json;
constexpr char kConfigurationFileName[] = "category-json-template.json";
constexpr std::string_view kPassengerBaggageBeltFamily = "PaxBaggageBelt";

std::string Lower(std::string_view value)
{
    std::string result(value);
    std::ranges::transform(result, result.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
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
        } else if (patternIndex < lowerPattern.size() &&
                   lowerPattern[patternIndex] == '*') {
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
    return value.at(keyName).get<double>();
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
        if (title.empty()) continue;
        if (seen.insert(Lower(title)).second) uniqueTitles.push_back(title);
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
        family.usingAlternate = preferred.empty() && !alternate.empty();
        family.resolvedTitles = preferred.empty() ? std::move(alternate) : std::move(preferred);
        if (family.resolvedTitles.empty()) {
            messages.push_back("Family '" + name + "' resolved to no installed SimObjects.");
        } else {
            messages.push_back("Family '" + name + "' resolved to " +
                               std::to_string(family.resolvedTitles.size()) +
                               (family.usingAlternate ? " alternate" : " preferred") +
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

    for (const Element &element : categoryIt->second) {
        auto family = m_families.find(element.family);
        bool walking = element.family == "WalkingWorker";
        if (family == m_families.end()) continue;
        if (family->second.resolvedTitles.empty() && walking) {
            family = m_families.find("StandingWorker");
            walking = false;
        }
        if (family == m_families.end() || family->second.resolvedTitles.empty()) continue;

        std::uniform_int_distribution<std::size_t> titleIndex(
            0, family->second.resolvedTitles.size() - 1);
        GroundServiceRequest request{};
        request.family = element.family;
        request.title = family->second.resolvedTitles[titleIndex(random)];
        if (element.family == kPassengerBaggageBeltFamily) {
            destination.push_back(std::move(request));
            continue;
        }

        const auto location = m_locations.find(element.location);
        if (location == m_locations.end()) continue;
        std::uniform_real_distribution<double> offsetX(
            -location->second.randomOffsetX, location->second.randomOffsetX);
        std::uniform_real_distribution<double> offsetY(
            -location->second.randomOffsetY, location->second.randomOffsetY);
        request.walking = walking;
        request.faceAircraft = location->second.faceAircraft;
        request.relX1 = location->second.relX1 + offsetX(random);
        request.relY1 = location->second.relY1 + offsetY(random);
        request.relX2 = location->second.relX2 + offsetX(random);
        request.relY2 = location->second.relY2 + offsetY(random);
        destination.push_back(std::move(request));
    }
}

void GroundServicesConfig::ClearResolution()
{
    for (auto &[name, family] : m_families) {
        family.resolvedTitles.clear();
        family.usingAlternate = false;
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

void GroundServicesConfig::Load(const std::filesystem::path &path)
{
    m_path = path;
    try {
        std::ifstream input(path);
        if (!input) throw std::runtime_error("could not open the configuration file");
        const Json document = Json::parse(input);
        if (!document.is_object()) throw std::runtime_error("root value must be an object");

        const Json &families = document.at("Families");
        if (!families.is_array()) throw std::runtime_error("'Families' must be an array");
        for (const Json &value : families) {
            if (!value.is_object()) throw std::runtime_error("each family must be an object");
            Family family{};
            family.name = ReadName(value, "Family");
            family.preferredPatterns = ReadStringArray(value, "PreferedObj", true);
            family.alternatePatterns = ReadStringArray(value, "AlternateObj", false);
            family.excludePatterns = ReadStringArray(value, "Exclude", false);
            if (!m_families.emplace(family.name, std::move(family)).second) {
                throw std::runtime_error("duplicate family name");
            }
        }

        const Json &locations = document.at("Locations");
        if (!locations.is_array()) throw std::runtime_error("'Locations' must be an array");
        for (const Json &value : locations) {
            if (!value.is_object()) throw std::runtime_error("each location must be an object");
            Location location{};
            location.name = ReadName(value, "Name");
            if (value.contains("RandOffset")) {
                throw std::runtime_error("location '" + location.name +
                                         "' uses obsolete RandOffset; use RandOffsetX and "
                                         "RandOffsetY");
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
            const bool hasWalking = value.contains("RelX1") || value.contains("RelY1") ||
                                    value.contains("RelX2") || value.contains("RelY2");
            if (hasStatic == hasWalking) {
                throw std::runtime_error("location must define either RelX/RelY or both endpoints");
            }
            location.walking = hasWalking;
            if (hasWalking) {
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
                    const bool aircraftAttached =
                        element.family == kPassengerBaggageBeltFamily;
                    if (aircraftAttached) {
                        if (entry.contains("Location")) {
                            throw std::runtime_error(
                                "PaxBaggageBelt is aircraft-attached and must not specify Location");
                        }
                    } else {
                        element.location = ReadName(entry, "Location");
                    }
                    const auto family = m_families.find(element.family);
                    if (family == m_families.end()) {
                        throw std::runtime_error("category references unknown family '" +
                                                 element.family + "'");
                    }
                    const auto location = m_locations.find(element.location);
                    if (!aircraftAttached && location == m_locations.end()) {
                        throw std::runtime_error("category references unknown location '" +
                                                 element.location + "'");
                    }
                    const bool walking = element.family == "WalkingWorker";
                    if (!aircraftAttached && walking != location->second.walking) {
                        throw std::runtime_error("WalkingWorker requires a two-endpoint location; "
                                                 "other families require RelX/RelY");
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
