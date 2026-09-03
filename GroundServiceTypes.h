#pragma once

#include <optional>
#include <string>
#include <vector>

namespace parking_services
{
// Shared, domain-neutral ground-service data types. These are pure data and are
// consumed by several layers (GroundServicesConfig, GSObject, AnimationThread),
// so they live here instead of in GroundServicesConfig.h. That keeps the
// GroundServicesConfig class (and the SimObjectCatalog it owns) the sole concern
// of GroundServicesThread, while every other layer stays agnostic to the config
// itself and simply uses these neutral types.

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
} // namespace parking_services