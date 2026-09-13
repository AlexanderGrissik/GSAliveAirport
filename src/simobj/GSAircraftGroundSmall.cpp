#include "GSAircraftGroundSmall.h"
#include "GSCateringCartMSFS.h"
#include "GSGroundPowerMSFS.h"

namespace NS_GSLiveAirportMSFS
{

void GSAircraftGroundSmall::BuildObjs()
{
    m_objs.emplace_back(new GSGroundPowerMSFS(m_simConnect, m_aircraft, *this));
}

}