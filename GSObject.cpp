#include "GSObject.h"

#include "GSLuggageLoaderFSDT.h"
#include "GSWalker.h"

#include <utility>

namespace parking_services
{
GSObject::GSObject(std::uint64_t token, AircraftSnapshot aircraft,
                   GroundServiceObject object, GroundServiceLocation location,
                   AircraftId parentObjectId)
    : m_token(token), m_aircraft(std::move(aircraft)), m_object(std::move(object)),
      m_location(location), m_parentObjectId(parentObjectId)
{
}

std::unique_ptr<GSObject> GSObject::Create(std::uint64_t token,
                                           const AircraftSnapshot &aircraft,
                                           const GroundServiceRequest &request)
{
    switch (request.specialType)
    {
    case GroundServiceSpecialType::LuggageLoaderFSDT:
        return std::make_unique<GSLuggageLoaderFSDT>(token, aircraft, request.object,
                                                      request.location);
    case GroundServiceSpecialType::WalkerFSDT:
        return std::make_unique<GSWalker>(token, aircraft, request.object, request.location);
    case GroundServiceSpecialType::None:
    default:
        return std::make_unique<GSObject>(token, aircraft, request.object, request.location,
                                          /*parentObjectId*/ 0);
    }
}

bool GSObject::PreparePlacement(GSObjectServices &services)
{
    if (m_location.kind != GroundServiceLocationKind::Static)
    {
        services.log("Skipped " + m_object.family + " for aircraft " +
                     std::to_string(m_aircraft.objectId) +
                     ": a plain ground-service object only supports static placement.");
        return false;
    }
    m_pose = RelativeToAircraft(m_aircraft, m_location.relX1, m_location.relY1,
                                m_location.faceAircraft);
    return true;
}

void GSObject::PrepareAttachment(const GSObjectSpawnPose &parentPose)
{
    m_pose = RelativeToParent(parentPose, m_object);
}

void GSObject::OnCreated(GSObjectServices &services, AircraftId objectId)
{
    m_createResolved = true;
    m_objectId = objectId;
    if (objectId == 0)
    {
        m_cancelled = true;
        services.log("Failed to create " + m_object.title + " for aircraft " +
                     std::to_string(m_aircraft.objectId) + ".");
        return;
    }
    if (m_cancelled || (m_parentObjectId != 0 && !services.isParentCreated(m_parentObjectId)))
    {
        m_cancelled = true;
        services.removeSimObject(objectId);
        return;
    }
    services.registerObject(this, objectId, m_pose);
    Activate(services, objectId);
}

void GSObject::Maintain(GSObjectServices &services, std::chrono::steady_clock::time_point now)
{
    static_cast<void>(services);
    static_cast<void>(now);
}

void GSObject::OnGeometry(GSObjectServices &services, const BaggageLoaderGeometry &geometry)
{
    static_cast<void>(services);
    static_cast<void>(geometry);
}

void GSObject::OnRemoved(GSObjectServices &services)
{
    static_cast<void>(services);
}

void GSObject::Activate(GSObjectServices &services, AircraftId objectId)
{
    static_cast<void>(objectId);
    Finish(services, m_pose);
}

void GSObject::Finish(GSObjectServices &services, const GSObjectSpawnPose &actualPose)
{
    m_pose = actualPose;
    services.finalize(this, actualPose);
    m_finalized = true;
}

GSObjectSpawnPose GSObject::RelativeToAircraft(const AircraftSnapshot &aircraft, double relX,
                                               double relY, bool faceAircraft)
{
    const auto position = RelativePosition(aircraft.headingDegrees, aircraft.longitude,
                                           aircraft.latitude, aircraft.groundAltitudeFeet,
                                           relY, relX);
    GSObjectSpawnPose result{position.Latitude, position.Longitude, position.Altitude,
                             position.Heading, true};
    if (faceAircraft)
    {
        result.headingDegrees = HeadingTowardRelativeOrigin(aircraft.headingDegrees, relY, relX);
    }
    return result;
}

GSObjectSpawnPose GSObject::RelativeToParent(const GSObjectSpawnPose &parent,
                                             const GroundServiceObject &child)
{
    // XYZH deliberately uses a different compact convention from Locations:
    // X is forward and -Y is right. Thus a parent at 090 with [0,-5,0,H]
    // places the child five metres south, at an absolute direction of 180.
    const auto position = RelativePosition(parent.headingDegrees, parent.longitude,
                                           parent.latitude, parent.altitudeFeet,
                                           child.parentX, -child.parentY);
    return {position.Latitude, position.Longitude,
            parent.altitudeFeet + child.parentZ / kFeetToMeters,
            NormalizeDegrees(parent.headingDegrees + child.parentHeadingDegrees),
            parent.onGround && std::abs(child.parentZ) < 0.001};
}

SIMCONNECT_DATA_INITPOSITION GSObject::ToInitialPosition(const GSObjectSpawnPose &pose)
{
    SIMCONNECT_DATA_INITPOSITION result{};
    result.Latitude = pose.latitude;
    result.Longitude = pose.longitude;
    result.Altitude = pose.altitudeFeet;
    result.Heading = pose.headingDegrees;
    result.OnGround = pose.onGround ? 1 : 0;
    return result;
}
} // namespace parking_services