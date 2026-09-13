#include "GSAircraftGroundMedium.h"
#include "GSCateringCartMSFS.h"
#include "GSGroundPowerMSFS.h"
#include "GSStatic.h"
#include "GSLinerCones.h"

namespace NS_GSLiveAirportMSFS
{

void GSAircraftGroundMedium::BuildObjs()
{
	m_objs.emplace_back(new GSCateringCartMSFS(m_simConnect, m_aircraft, *this));
	m_objs.emplace_back(new GSLinerCones(m_simConnect, m_aircraft, *this));

	auto ptr = new GSGroundPowerMSFS(m_simConnect, m_aircraft, *this);
	ptr->SetGPUType(GSGroundPowerMSFS::GPU_MEDIUM);
    m_objs.emplace_back(ptr);
}

}