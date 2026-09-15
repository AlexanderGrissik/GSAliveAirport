#include "GSAircraftGroundXL.h"
#include "GSCateringCartMSFS.h"
#include "GSGroundPowerMSFS.h"
#include "GSLinerCones.h"
#include "GSPushback.h"
#include "GSLavatoryTruck.h"
#include "GSBuggageLoader.h"

namespace NS_GSLiveAirportMSFS
{

void GSAircraftGroundXL::BuildObjs()
{
	m_objs.emplace_back(new GSCateringCartMSFS(m_simConnect, m_aircraft, *this));
	m_objs.emplace_back(new GSLinerCones(m_simConnect, m_aircraft, *this));
    m_objs.emplace_back(new GSGroundPowerMSFS(m_simConnect, m_aircraft, *this, GSGroundPowerMSFS::GPU_DEFAULT));
	m_objs.emplace_back(new GSPushback(m_simConnect, m_aircraft, *this, GSPushback::PUSH_XL));
    m_objs.emplace_back(new GSLavatoryTruck(m_simConnect, m_aircraft, *this));
	m_objs.emplace_back(new GSBuggageLoader(m_simConnect, m_aircraft, *this, true));
	m_objs.emplace_back(new GSBuggageLoader(m_simConnect, m_aircraft, *this, false));
}

}