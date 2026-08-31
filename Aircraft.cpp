#include "Aircraft.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

namespace parking_services
{
namespace
{
constexpr std::array<double, 10> kMetersPerDegree{{
    111'000.0, //  0-10 degrees latitude
    109'000.0, // 10-20 degrees latitude
    106'000.0, // 20-30 degrees latitude
    100'000.0, // 30-40 degrees latitude
     95'000.0, // 40-50 degrees latitude
     87'500.0, // 50-60 degrees latitude
     79'000.0, // 60-70 degrees latitude
     70'000.0, // 70-80 degrees latitude
     60'000.0, // 80-90 degrees latitude
     56'000.0, // 90 degrees latitude
}};
}

std::string NormalizeTrafficState(std::string state)
{
    std::ranges::transform(state, state.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    std::ranges::replace(state, '_', ' ');
    if (state.starts_with("state ")) {
        state.erase(0, 6);
    }
    return state;
}

std::optional<AircraftSizeCategory> ClassifyAircraftSize(double wingSpanMeters)
{
    if (wingSpanMeters < 0.0) return std::nullopt;
    if (wingSpanMeters < 24.0) return AircraftSizeCategory::Small;
    if (wingSpanMeters < 36.0) return AircraftSizeCategory::Medium;
    if (wingSpanMeters < 65.0) return AircraftSizeCategory::Large;
    return AircraftSizeCategory::ExtraLarge;
}

std::string_view AircraftSizeCategoryName(AircraftSizeCategory category)
{
    switch (category) {
    case AircraftSizeCategory::Small:
        return "Small";
    case AircraftSizeCategory::Medium:
        return "Medium";
    case AircraftSizeCategory::Large:
        return "Large";
    case AircraftSizeCategory::ExtraLarge:
        return "ExtraLarge";
    }
    return "Unknown";
}

double MetersPerDegree(double latitude)
{
    const auto latitudeBand = (std::min)(
        static_cast<std::size_t>(std::abs(latitude) / 10.0),
        kMetersPerDegree.size() - 1);
    return kMetersPerDegree[latitudeBand];
}

double DistanceMeters(double latitudeA, double longitudeA,
                      double latitudeB, double longitudeB)
{
    const double latitudeDelta = latitudeB - latitudeA;
    const double longitudeDelta = longitudeB - longitudeA;
    const double distanceDegrees = std::sqrt(
        latitudeDelta * latitudeDelta + longitudeDelta * longitudeDelta);
    return distanceDegrees * MetersPerDegree(latitudeA);
}
} // namespace parking_services
