#include "GSObject.h"

#include "GSCommon.h"
#include "GSLuggageLoaderFSDT.h"
#include "GSPowerGround.h"
#include "GSStaticObj.h"
#include "GSWalkerFSDT.h"

#include <algorithm>
#include <utility>

namespace parking_services
{
GSObject::GSObject(AircraftSnapshot aircraft, GroundServiceObject object,
                   GroundServiceLocation location)
    : m_aircraft(std::move(aircraft)), m_object(std::move(object)),
      m_location(location)
{
}

std::unique_ptr<GSObject> GSObject::Create(const AircraftSnapshot &aircraft,
                                           const GroundServiceRequest &request)
{
    switch (request.specialType)
    {
    case GroundServiceSpecialType::LuggageLoaderFSDT:
        return std::make_unique<GSLuggageLoaderFSDT>(aircraft, request.object,
                                                      request.location);
    case GroundServiceSpecialType::WalkerFSDT:
        return std::make_unique<GSWalkerFSDT>(aircraft, request.object,
                                              request.location);
    case GroundServiceSpecialType::GroundPowerDefault:
        return std::make_unique<GSPowerGround>(aircraft, request.object,
                                               request.location);
    case GroundServiceSpecialType::None:
    default:
        return std::make_unique<GSStaticObj>(aircraft, request.object,
                                             request.location);
    }
}

std::unique_ptr<GSObject> GSObject::CreateAttachment(
    const AircraftSnapshot &aircraft, const GroundServiceObject &object)
{
    return std::make_unique<GSStaticObj>(aircraft, object, GroundServiceLocation{});
}

bool GSObject::ReadyToDestroy() const
{
    return OwnRequestsFinished() &&
           std::ranges::all_of(m_children, [](const auto &child) {
               return child->ReadyToDestroy();
           });
}

bool GSObject::ReadyForRemoval() const
{
    const bool ownLifecycleComplete =
        m_createResolved && (m_objectId == 0 || m_finalized);
    return ownLifecycleComplete && OwnRequestsFinished() &&
           std::ranges::all_of(m_children, [](const auto &child) {
               return child->ReadyForRemoval();
           });
}

GSObject &GSObject::AddChild(std::unique_ptr<GSObject> child)
{
    child->m_parent = this;
    GSObject &reference = *child;
    m_children.push_back(std::move(child));
    return reference;
}

GSObject *GSObject::FindByObjectId(AircraftId objectId)
{
    if (m_objectId == objectId) return this;
    for (const auto &child : m_children) {
        if (GSObject *found = child->FindByObjectId(objectId)) return found;
    }
    return nullptr;
}

std::unique_ptr<GSObject> GSObject::ReleaseDescendant(AircraftId objectId)
{
    for (auto child = m_children.begin(); child != m_children.end(); ++child) {
        if ((*child)->ObjectId() == objectId) {
            std::unique_ptr<GSObject> result = std::move(*child);
            m_children.erase(child);
            result->m_parent = nullptr;
            return result;
        }
        if (auto result = (*child)->ReleaseDescendant(objectId)) return result;
    }
    return {};
}

void GSObject::MaintainTree(
    GSObjectServices &services, std::chrono::steady_clock::time_point now)
{
    std::erase_if(m_commandRequests, [](const auto &request) {
        return request->IsFinished();
    });
    if (m_createResolved && m_objectId != 0 && !m_finalized) {
        Maintain(services, now);
    }
    for (const auto &child : m_children) child->MaintainTree(services, now);
    std::erase_if(m_children, [](const auto &child) {
        return child->Retired() && child->ReadyToDestroy();
    });
}

void GSObject::ConfigureSimConnect(GSObjectServices &services)
{
    static_cast<void>(services);
}

GSReqCommand &GSObject::NewCommandRequest()
{
    auto request = std::make_unique<GSReqCommand>();
    GSReqCommand &reference = *request;
    m_commandRequests.push_back(std::move(request));
    return reference;
}

bool GSObject::SpecialRequestsFinished() const
{
    return true;
}

bool GSObject::OwnRequestsFinished() const
{
    if (!SpecialRequestsFinished()) return false;
    return std::ranges::all_of(m_commandRequests, [](const auto &request) {
        return request->IsFinished();
    });
}

void GSObject::PrepareAttachment(const GSObject::GSObjectPos &parentPose)
{
    m_pose = RelativeToParent(parentPose, m_object);
}

void GSObject::OnCreated(GSObjectServices &services, AircraftId objectId)
{
    m_createResolved = true;
    m_objectId = objectId;
    if (objectId == 0)
    {
        GSLog("Failed to create " + m_object.title + " for aircraft " +
              std::to_string(m_aircraft.objectId) + ".");
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

void GSObject::OnRemoved(GSObjectServices &services)
{
    static_cast<void>(services);
}

bool GSObject::RepositionRelative(GSObjectServices &services, double x,
                                  double y, double z,
                                  double headingDegrees)
{
    static_cast<void>(services);
    static_cast<void>(x);
    static_cast<void>(y);
    static_cast<void>(z);
    static_cast<void>(headingDegrees);
    return false;
}

void GSObject::Activate(GSObjectServices &services, AircraftId objectId)
{
    static_cast<void>(objectId);
    Finish(services, m_pose);
}

void GSObject::OnParentRepositioned(GSObjectServices &services,
                                    const GSObjectPos &parentPose)
{
    static_cast<void>(services);
    static_cast<void>(parentPose);
}

void GSObject::RepositionChildren(GSObjectServices &services)
{
    for (const auto &child : m_children) {
        child->OnParentRepositioned(services, m_pose);
    }
}

void GSObject::Finish(GSObjectServices &services, const GSObject::GSObjectPos &actualPose)
{
    m_pose = actualPose;
    services.finalize(this, actualPose);
    m_finalized = true;
}

GSObject::GSObjectPos GSObject::RelativeToAircraft(const AircraftSnapshot &aircraft, double relX,
                                         double relY, bool faceAircraft,
                                         bool faceAircraftReverse, bool wingRelative)
{
    // When the location is authored in wingspans, scale every offset (base and jitter)
    // by the aircraft's reported wingspan before converting to world coordinates.
    if (wingRelative && aircraft.wingSpanMeters > 0.0)
    {
        relX *= aircraft.wingSpanMeters;
        relY *= aircraft.wingSpanMeters;
    }
    const auto position = RelativePosition(aircraft.headingDegrees, aircraft.longitude,
                                           aircraft.latitude, aircraft.groundAltitudeFeet,
                                           relY, relX);
    GSObject::GSObjectPos result{position.Latitude, position.Longitude, position.Altitude,
                       position.Heading, true};
    if (faceAircraft)
    {
        result.headingDegrees = HeadingTowardRelativeOrigin(aircraft.headingDegrees, relY, relX);
        if (faceAircraftReverse) result.headingDegrees = NormalizeDegrees(result.headingDegrees + 180.0);
    }
    return result;
}

GSObject::GSObjectPos GSObject::RelativeToParent(const GSObject::GSObjectPos &parent,
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

SIMCONNECT_DATA_INITPOSITION GSObject::ToInitialPosition(const GSObject::GSObjectPos &pose)
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
