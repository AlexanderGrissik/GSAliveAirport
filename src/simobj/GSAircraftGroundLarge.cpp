#include "GSAircraftGroundLarge.h"
#include "GSCateringCartMSFS.h"
#include "GSGroundPowerMSFS.h"

namespace NS_GSLiveAirportMSFS
{

void GSAircraftGroundLarge::BuildObjs()
{
	m_objs.emplace_back(new GSCateringCartMSFS(m_simConnect, m_aircraft, *this));
    m_objs.emplace_back(new GSGroundPowerMSFS(m_simConnect, m_aircraft, *this));
}

}