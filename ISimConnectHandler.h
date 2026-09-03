#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#pragma warning(push)
#pragma warning(disable : 4245) // SimConnect SDK enum declarations use signed literals.
#include <SimConnect.h>
#pragma warning(pop)

#include <string>

namespace parking_services
{
class ISimConnectRequest;

// Thread-safe command surface for the SimConnect dispatch thread. Every SDK
// operation receives its own request object. The originator owns that object
// until OnSuccess or OnFailure marks it finished. SimConnectThread owns only
// request/send IDs and keeps non-owning references while routing SDK messages.
class ISimConnectHandler
{
  public:
    virtual ~ISimConnectHandler() = default;

    virtual void Stop() = 0;

    virtual void EnumerateObjects(SIMCONNECT_SIMOBJECT_TYPE type,
                                  ISimConnectRequest &request) = 0;
    virtual void RequestObjectData(SIMCONNECT_DATA_DEFINITION_ID definition, DWORD objectId,
                                   SIMCONNECT_PERIOD period, DWORD interval,
                                   ISimConnectRequest &request) = 0;
    virtual void RequestObjectDataByType(SIMCONNECT_DATA_DEFINITION_ID definition,
                                         DWORD radiusMeters, SIMCONNECT_SIMOBJECT_TYPE type,
                                         ISimConnectRequest &request) = 0;

    virtual void CreateObject(std::string title, SIMCONNECT_DATA_INITPOSITION position,
                              ISimConnectRequest &request) = 0;
    virtual void RemoveObject(DWORD objectId,
                              ISimConnectRequest &request) = 0;

    // The handler copies data before posting the operation, so the caller's
    // buffer does not need to outlive this call.
    virtual void SetObjectData(SIMCONNECT_DATA_DEFINITION_ID definition, DWORD objectId,
                               DWORD arrayCount, DWORD elementSize, const void *data,
                               ISimConnectRequest &request) = 0;
    virtual void AddDatum(SIMCONNECT_DATA_DEFINITION_ID definition,
                          std::string datumName, std::string units,
                          SIMCONNECT_DATATYPE type,
                          ISimConnectRequest &request) = 0;
    virtual void MapClientEvent(SIMCONNECT_CLIENT_EVENT_ID eventId,
                                std::string eventName,
                                ISimConnectRequest &request) = 0;

    virtual void TransmitEvent(DWORD objectId, SIMCONNECT_CLIENT_EVENT_ID eventId, DWORD data,
                               ISimConnectRequest &request) = 0;
    virtual void TransmitEventEx1(DWORD objectId, SIMCONNECT_CLIENT_EVENT_ID eventId,
                                  DWORD data0, DWORD data1,
                                  ISimConnectRequest &request) = 0;
};
} // namespace parking_services
