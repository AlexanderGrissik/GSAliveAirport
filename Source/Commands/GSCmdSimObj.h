#pragma once

#include "../SimObjects/GSSimObj.h"
#include "../General/GSSimConnect.h"

namespace NS_GSLiveAirportMSFS
{

class GSCmdSimObj : public GSCommand
{
public:

    GSCmdSimObj(int cmd, GSSimObj& simObj) : GSCommand(cmd), m_simObj(simObj) {}
    virtual ~GSCmdSimObj() {}

    GSSimObj& SimObj() { return m_simObj; }
        
private:

    GSSimObj& m_simObj;
};

} // namespace NS_GSLiveAirportMSFS
