#include "GSAircraftGroundXL.h"
#include "GSCateringCartMSFS.h"
#include "GSGroundPowerMSFS.h"
#include "GSLinerCones.h"
#include "GSPushback.h"

namespace NS_GSLiveAirportMSFS
{

void GSAircraftGroundXL::BuildObjs()
{
	m_objs.emplace_back(new GSCateringCartMSFS(m_simConnect, m_aircraft, *this));
	m_objs.emplace_back(new GSLinerCones(m_simConnect, m_aircraft, *this));
    m_objs.emplace_back(new GSGroundPowerMSFS(m_simConnect, m_aircraft, *this, GSGroundPowerMSFS::GPU_DEFAULT));
	m_objs.emplace_back(new GSPushback(m_simConnect, m_aircraft, *this, GSPushback::PUSH_XL));
}

}