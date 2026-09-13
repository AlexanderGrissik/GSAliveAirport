#include "GSAircraftGroundMedium.h"
#include "GSCateringCartMSFS.h"

namespace NS_GSLiveAirportMSFS
{

void GSAircraftGroundMedium::BuildObjs()
{
	m_objs.emplace_back(new GSCateringCartMSFS(m_simConnect, m_aircraft, *this));
}

}