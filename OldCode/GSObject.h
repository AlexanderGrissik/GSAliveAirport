#pragma once

#include "GSAircraft.h"
#include "AnimatedObject.h"
#include "GSRequests/GSReqCommand.h"
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
class ISimConnectHandler;

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
    LuggageLoaderFrontFSDT,
    LuggageLoaderBackFSDT,
    WalkerFSDT,
    WorkerFSDT,
    GroundPowerDefault,
    CateringDefault
};

enum class GroundServiceLocationKind
{
    Static,
    Route
};

enum class GroundServiceLocationRelation
{
    Aircraft,
    PushbackContact,
    RearRightDoor
};

struct GroundServiceLocation
{
    GroundServiceLocationKind kind{GroundServiceLocationKind::Static};
    bool faceAircraft{};
    bool faceAircraftReverse{};
    double relX1{};
    double relY1{};
    double relX2{};
    double relY2{};
    bool wingRelative{};
    GroundServiceLocationRelation relation{GroundServiceLocationRelation::Aircraft};
};

struct GroundServiceRequest
{
    GroundServiceObject object;
    GroundServiceLocation location;
    // Carried through so the ground-service object factory can dispatch to the
    // right GSObject subclass (WalkerFSDT / luggage-loader variants / plain).
    GroundServiceSpecialType specialType{GroundServiceSpecialType::None};
};

struct GSObjectServices; // forward-declared for GSObject's capability references.

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

    GSObject(AircraftSnapshot aircraft, GroundServiceObject object,
             GroundServiceLocation location);
    virtual ~GSObject() = default;

    // Factory: constructs the GSObject subclass matching the request's special
    // type. This is the single place the concrete types are named, so the driver
    // (and everything else) can stay agnostic.
    static std::unique_ptr<GSObject> Create(const AircraftSnapshot &aircraft,
                                            const GroundServiceRequest &request);
    static std::unique_ptr<GSObject> CreateAttachment(
        const AircraftSnapshot &aircraft, const GroundServiceObject &object);

    [[nodiscard]] AircraftId AircraftObjectId() const { return m_aircraft.objectId; }
    [[nodiscard]] AircraftId ObjectId() const { return m_objectId; }
    [[nodiscard]] const GroundServiceObject &Object() const { return m_object; }
    [[nodiscard]] const GSObjectPos &Pose() const { return m_pose; }
    [[nodiscard]] const std::vector<AnimationCoordinate> &MovementCoordinates() const
    {
        return m_movementCoordinates;
    }
    [[nodiscard]] double MovementSpeedMetersPerSecond() const
    {
        return m_movementSpeedMetersPerSecond;
    }
    // FSDT workers use VELOCITY BODY Y as an animation-frame carrier. For a
    // stationary worker it must be written with the fixed world pose, using
    // the same direct-position packet as a moving worker.
    [[nodiscard]] virtual bool UsesPositionedVelocityAnimation() const
    {
        return false;
    }
    [[nodiscard]] const AircraftSnapshot &Aircraft() const { return m_aircraft; }
    // True once the async create has resolved (either produced an object or
    // failed). Until then the object is still in flight and must be retained.
    [[nodiscard]] bool Resolved() const { return m_createResolved; }
    [[nodiscard]] bool Finalized() const { return m_finalized; }
    // A created object is removable only after subtype-specific finalization
    // and all of its outstanding SimConnect requests have settled.
    [[nodiscard]] bool ReadyForRemoval() const;
    // True when creation settled without producing a sim object to manage.
    [[nodiscard]] bool Retired() const
    {
        return m_createResolved && m_objectId == 0;
    }

    [[nodiscard]] bool ReadyToDestroy() const;

    GSObject &AddChild(std::unique_ptr<GSObject> child);
    [[nodiscard]] const std::vector<std::unique_ptr<GSObject>> &Children() const
    {
        return m_children;
    }
    [[nodiscard]] GSObject *FindByObjectId(AircraftId objectId);
    [[nodiscard]] std::unique_ptr<GSObject> ReleaseDescendant(AircraftId objectId);
    void MaintainTree(GSObjectServices &services,
                      std::chrono::steady_clock::time_point now);

    // Lifecycle hooks driven by GroundServicesThread (all run on the GS thread).
    // PreparePlacement computes this object's pose/route/attachment from its
    // configured location and returns false if it cannot be placed.
    virtual bool PreparePlacement(GSObjectServices &services) = 0;
    // Allows a concrete object to define the SimConnect data/events it needs.
    // GroundServicesThread calls this generically before submitting creation.
    virtual void ConfigureSimConnect(GSObjectServices &services);
    // Computes a child's pose relative to an already-created parent object.
    virtual void PrepareAttachment(const GSObjectPos &parentPose);
    virtual void OnCreated(GSObjectServices &services, AircraftId objectId);
    virtual void Maintain(GSObjectServices &services,
                          std::chrono::steady_clock::time_point now);
    virtual void OnRemoved(GSObjectServices &services);

    // Shared world-coordinate pose math.
    [[nodiscard]] static GSObjectPos RelativeToAircraft(
        const AircraftSnapshot &aircraft, double relX, double relY, bool faceAircraft,
        bool faceAircraftReverse = false, bool wingRelative = false,
        GroundServiceLocationRelation relation = GroundServiceLocationRelation::Aircraft);
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
    GSReqCommand &NewCommandRequest();
    [[nodiscard]] bool LocationRelationAvailable() const;
    [[nodiscard]] virtual bool SpecialRequestsFinished() const;
    [[nodiscard]] bool OwnRequestsFinished() const;

    AircraftSnapshot m_aircraft;
    GroundServiceObject m_object;
    GroundServiceLocation m_location;
    GSObjectPos m_pose{};
    std::vector<AnimationCoordinate> m_movementCoordinates;
    double m_movementSpeedMetersPerSecond{};
    AircraftId m_objectId{};
    bool m_createResolved{};
    bool m_finalized{};
    // Non-owning; the parent owns this object through m_children.
    GSObject *m_parent{};
    std::vector<std::unique_ptr<GSReqCommand>> m_commandRequests;
    std::vector<std::unique_ptr<GSObject>> m_children;
};

// Capabilities a GSObject uses from its driver (GroundServicesThread). Every
// member runs on the ground-services thread, so it may mutate shared state.
// The thread implements only generic lifecycle services and ID allocation;
// concrete objects use the handler directly for their own SimConnect behavior.
struct GSObjectServices
{
    ISimConnectHandler *simConnect{};
    std::function<SIMCONNECT_DATA_DEFINITION_ID()> allocateDataDefinition;
    std::function<SIMCONNECT_CLIENT_EVENT_ID()> allocateClientEvent;
    std::function<void(GSObject *, AircraftId, const GSObject::GSObjectPos &)> registerObject;
    std::function<void(GSObject *, const GSObject::GSObjectPos &)> finalize;
};
} // namespace parking_services
