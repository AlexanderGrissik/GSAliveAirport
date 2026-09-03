#include "GSWalker.h"

#include "GSCommon.h"

#include <utility>

namespace parking_services
{
GSWalker::GSWalker(std::uint64_t token, AircraftSnapshot aircraft,
                   GroundServiceObject object, GroundServiceLocation location)
    : GSObject(token, std::move(aircraft), std::move(object), std::move(location),
               /*parentObjectId*/ 0)
{
}

bool GSWalker::PreparePlacement(GSObjectServices &)
{
    if (m_location.kind != GroundServiceLocationKind::Route)
    {
        GSLog("Skipped " + m_object.family + " for aircraft " +
              std::to_string(m_aircraft.objectId) +
              ": a walking worker only supports route placement.");
        return false;
    }
    m_pose = RelativeToAircraft(m_aircraft, m_location.relX1, m_location.relY1, false);
    const GSObject::GSObjectPos endpoint =
        RelativeToAircraft(m_aircraft, m_location.relX2, m_location.relY2, false);
    m_movementCoordinates = {
        {m_pose.latitude, m_pose.longitude, m_pose.altitudeFeet,
         m_pose.headingDegrees},
        {endpoint.latitude, endpoint.longitude, endpoint.altitudeFeet,
         endpoint.headingDegrees}};
    return true;
}
} // namespace parking_services
