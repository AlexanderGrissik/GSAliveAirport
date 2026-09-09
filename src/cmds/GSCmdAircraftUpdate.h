#pragma once

#include "GSCommand.h"
#include "../GSAircraft.h"

namespace NS_GSLiveAirportMSFS
{

class GSCmdAircraftUpdate : public GSCommand
{
public:

    GSCmdAircraftUpdate(int cmd, std::shared_ptr<GSAircraft>& ac): GSCommand(cmd), m_aircraft(ac) {}
    virtual ~GSCmdAircraftUpdate() {}

    const std::shared_ptr<GSAircraft>& GetAircraft() const { return m_aircraft; }

private:

    std::shared_ptr<GSAircraft> m_aircraft;
};
} // namespace NS_GSLiveAirportMSFS