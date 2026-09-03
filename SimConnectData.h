#pragma once

namespace parking_services
{
// Raw SimConnect payload layouts for the baggage-loader path. These mirror the
// data definitions registered by the handler (DefinitionBaggageLoader* in
// SimConnectIds.h).
#pragma pack(push)
struct BaggageLoaderRampTargetWireData
{
    double angleDegrees{};
};

struct BaggageLoaderGeometryWireData
{
    double angleCurrentDegrees{};
    double endRampYMeters{};
    double endRampZMeters{};
    double pivotYMeters{};
    double pivotZMeters{};
};
#pragma pack(pop)

static_assert(sizeof(BaggageLoaderRampTargetWireData) == 8);
static_assert(sizeof(BaggageLoaderGeometryWireData) == 40);
} // namespace parking_services
