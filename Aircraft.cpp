#include "Aircraft.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace parking_services
{
namespace
{
constexpr double kMetersPerLatitudeDegree = 111'320.0;
constexpr double kDegreesToRadians = 3.14159265358979323846 / 180.0;
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

double MetersPerDegreeLat()
{
    return kMetersPerLatitudeDegree;
}

double MetersPerDegreeLong(double latitude)
{
    return kMetersPerLatitudeDegree *
           std::cos(latitude * kDegreesToRadians);
}

double DistanceMeters(double latitudeA, double longitudeA,
                      double latitudeB, double longitudeB)
{
    const double latitudeMeters =
        (latitudeB - latitudeA) * MetersPerDegreeLat();
    const double averageLatitude = (latitudeA + latitudeB) / 2.0;
    const double longitudeMeters =
        (longitudeB - longitudeA) * MetersPerDegreeLong(averageLatitude);
    return std::hypot(latitudeMeters, longitudeMeters);
}
} // namespace parking_services
