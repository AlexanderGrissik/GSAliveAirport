#pragma once

#include "../General/GSSimConnect.h"
#include "GSSimObj.h"

#include <chrono>
#include <memory>
#include <stop_token>
#include <thread>
#include <unordered_map>

namespace NS_GSLiveAirportMSFS
{

class GSMovementThread final : public GSSimConnect
{
public:

	#pragma pack(push, 1)
	struct StateWireDataGet
	{
		double posLong;
		double posLat;
	};
	#pragma pack(pop)

	void Start();
	void Stop();

	void OnConnect() override;
	void OnDisconnect() override {}
	void OnSimStart() override {}
	void OnSimStop() override {}
	void OnCommand(GSCommand& cmd) override;

	void IncrInflight() { ++m_inflightReqs; }
	void DecrInflight() { --m_inflightReqs; }

	void HandlePosMessage(SIMCONNECT_RECV_SIMOBJECT_DATA& entry, GSSimObj& obj);

private:

	const char* GetDebugName() const override { return "GSMovementThread"; }

	static void MoveTrackLoop(std::stop_token stopToken, GSMovementThread* self);
	void RunLoopMoveTracking(std::stop_token stopToken);
	void RequestTracking();
	void RemovePending();

	std::unordered_map<SIMCONNECT_OBJECT_ID, GSSimObj*> m_trackObjs;
	std::unordered_map<SIMCONNECT_OBJECT_ID, GSSimObj*> m_pendRemObjs;
	std::chrono::steady_clock::time_point m_lastTrackTime{};
	std::jthread m_thread;
	size_t m_inflightReqs = 0U;

	class GSReqObjPosition : public GSSimObj::GSSimObjReq {
	public:
		using GSSimObj::GSSimObjReq::GSSimObjReq;
		GSRequest::SendResult Process() override;
		bool OnMessage(SIMCONNECT_RECV* message, DWORD messageSize) override;
		void OnException(SIMCONNECT_RECV_EXCEPTION* message) override;
	};
};

}
