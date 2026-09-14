#include "GSAircraftGroundSmall.h"
#include "GSPushback.h"

namespace NS_GSLiveAirportMSFS
{

void GSAircraftGroundSmall::BuildObjs()
{
    m_objs.emplace_back(new GSPushback(m_simConnect, m_aircraft, *this, GSPushback::PUSH_SMALL));
}

}