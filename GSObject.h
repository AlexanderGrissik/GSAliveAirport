#pragma once

#include "Aircraft.h"
#include "AnimatedObject.h"
#include "SimObjectPositioning.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace parking_services
{
// A resolved configuration node used to create one simulator ground object.
struct GroundServiceObject
{
    std::string family;
    std::string title;
    std::optional<AnimationConfiguration> animation;

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

// Resolved cargo-door attachment data for a luggage loader.
struct GSObjectCargoDoor
{
    std::uint32_t interactivePointIndex{};
    double cargoForwardMeters{};
    double cargoRightMeters{};
    double cargoHeightMeters{};
    double modelRelativeHeadingDegrees{};
};

struct GSObjectServices; // forward-declared for GSObject's capability references.

// Result of a baggage-loader geometry read: the loader's current ramp angle and
// the pivot/end-ramp offsets (meters) the ground-services domain uses to align
// the ramp against an open cargo door.
struct BaggageLoaderGeometry
{
    bool succeeded{};
    double angleCurrentDegrees{};
    double endRampYMeters{};
    double endRampZMeters{};
    double pivotYMeters{};
    double pivotZMeters{};
};

// A single ground-service object request and its lifecycle. The base class owns
// the common handling: creation, finalization (animation + attachments),
// removal, and the world-coordinate pose math. Subclasses add special-type
// specifics keyed by GroundServiceSpecialType. GroundServicesThread drives every
// object through this base interface (and the GSObjectServices capabilities)
// without knowing the concrete type.
class GSObject
{
  public:
    // A spawn pose in world coordinates (feet/degrees), used for creation and motion.
    struct GSObjectPos
    {
        double latitude{};
        double longitude{};
        double altitudeFeet{};
        double headingDegrees{};
        bool onGround{true};
    };

    GSObject(std::uint64_t token, AircraftSnapshot aircraft,
             GroundServiceObject object, GroundServiceLocation location,
             AircraftId parentObjectId);
    virtual ~GSObject() = default;

    // Factory: constructs the GSObject subclass matching the request's special
    // type. This is the single place the concrete types are named, so the driver
    // (and everything else) can stay agnostic.
    static std::unique_ptr<GSObject> Create(std::uint64_t token,
                                            const AircraftSnapshot &aircraft,
                                            const GroundServiceRequest &request);

    [[nodiscard]] std::uint64_t Token() const { return m_token; }
    [[nodiscard]] AircraftId AircraftObjectId() const { return m_aircraft.objectId; }
    [[nodiscard]] AircraftId ObjectId() const { return m_objectId; }
    [[nodiscard]] AircraftId ParentObjectId() const { return m_parentObjectId; }
    [[nodiscard]] const GroundServiceObject &Object() const { return m_object; }
    [[nodiscard]] const GSObjectPos &Pose() const { return m_pose; }
    [[nodiscard]] const std::vector<AnimationCoordinate> &MovementCoordinates() const
    {
        return m_movementCoordinates;
    }
    [[nodiscard]] const AircraftSnapshot &Aircraft() const { return m_aircraft; }
    [[nodiscard]] const std::optional<GSObjectCargoDoor> &CargoDoor() const
    {
        return m_cargoDoor;
    }
    // True once the async create has resolved (either produced an object or
    // failed). Until then the object is still in flight and must be retained.
    [[nodiscard]] bool Resolved() const { return m_createResolved; }
    [[nodiscard]] bool Finalized() const { return m_finalized; }
    // True once the create has settled and there is nothing more to track: the
    // create was attempted and produced no sim object to manage.
    [[nodiscard]] bool Retired() const
    {
        return m_createResolved && m_objectId == 0;
    }

    void SetPose(const GSObjectPos &pose) { m_pose = pose; }
    void SetMovementCoordinates(std::vector<AnimationCoordinate> coordinates)
    {
        m_movementCoordinates = std::move(coordinates);
    }

    // Lifecycle hooks driven by GroundServicesThread (all run on the GS thread).
    // PreparePlacement computes this object's pose/route/attachment from its
    // configured location and returns false if it cannot be placed.
    virtual bool PreparePlacement(GSObjectServices &services);
    // Computes a child's pose relative to an already-created parent object.
    virtual void PrepareAttachment(const GSObjectPos &parentPose);
    virtual void OnCreated(GSObjectServices &services, AircraftId objectId);
    virtual void Maintain(GSObjectServices &services,
                          std::chrono::steady_clock::time_point now);
    virtual void OnGeometry(GSObjectServices &services, const BaggageLoaderGeometry &geometry);
    virtual void OnRemoved(GSObjectServices &services);

    // Shared world-coordinate pose math.
    [[nodiscard]] static GSObjectPos RelativeToAircraft(
        const AircraftSnapshot &aircraft, double relX, double relY, bool faceAircraft);
    [[nodiscard]] static GSObjectPos RelativeToParent(const GSObjectPos &parent,
                                                      const GroundServiceObject &child);
    [[nodiscard]] static SIMCONNECT_DATA_INITPOSITION ToInitialPosition(
        const GSObjectPos &pose);

  protected:
    // Post-creation hook. The base finalizes immediately; subclasses override to
    // run type-specific work (e.g. cargo-door ramp alignment) before finishing.
    virtual void Activate(GSObjectServices &services, AircraftId objectId);
    // Shared completion: record the final pose, add animation + attachments, and
    // mark the object done.
    void Finish(GSObjectServices &services, const GSObjectPos &actualPose);

    std::uint64_t m_token{};
    AircraftSnapshot m_aircraft;
    GroundServiceObject m_object;
    GroundServiceLocation m_location;
    AircraftId m_parentObjectId{};
    GSObjectPos m_pose{};
    std::vector<AnimationCoordinate> m_movementCoordinates;
    std::optional<GSObjectCargoDoor> m_cargoDoor;
    AircraftId m_objectId{};
    bool m_createResolved{};
    bool m_finalized{};
};

// Capabilities a GSObject uses from its driver (GroundServicesThread). Every
// member runs on the ground-services thread, so it may mutate shared state.
// The thread implements this set; GSObject subclasses call into it instead of
// touching SimConnect/animation directly, which keeps them decoupled and lets
// the driver stay agnostic to the concrete object type.
struct GSObjectServices
{
    std::function<void(GSObject *, AircraftId, const GSObject::GSObjectPos &)> registerObject;
    std::function<void(GSObject *, const GSObject::GSObjectPos &)> finalize;
    std::function<void(AircraftId)> removeSimObject;
    std::function<void(AircraftId, const GSObject::GSObjectPos &)> setPosition;
    std::function<void(AircraftId)> freezeObject;
    std::function<void(AircraftId, std::uint32_t)> openCargoDoor;
    std::function<void(AircraftId, std::uint32_t)> closeCargoDoor;
    std::function<void(AircraftId, double)> setRampTarget;
    std::function<void(AircraftId)> requestBaggageGeometry;
    std::function<bool(AircraftId)> isParentCreated;
};
} // namespace parking_services
