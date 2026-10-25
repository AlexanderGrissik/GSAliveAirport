// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSAnimationObject.h"
#include "../General/GSSimConnect.h"
#include "../SimObjects/GSAircraft.h"

#include <chrono>
#include <memory>
#include <stop_token>
#include <thread>
#include <unordered_map>

namespace NS_GSAliveAirport
{

class GSAnimationThread final : public GSSimConnect
{
public:

	void Start();
	void Stop();

	void OnConnect() override;
	void OnDisconnect() override {}
	void OnSimStart() override {}
	void OnSimStop() override {}
	void OnCommand(GSCommand& cmd) override;

private:

	const char* GetDebugName() const override { return "GSAnimationThread"; }

	static void AnimationLoop(std::stop_token stopToken, GSAnimationThread* self);
	void RunLoopAnimation(std::stop_token stopToken);
	void DoAnimation();
	void UserAircraft(const GSAircraft& aircraft);

	std::unordered_map<SIMCONNECT_OBJECT_ID, std::unique_ptr<GSAnimationObject>> m_animObjs;
	GSTimePoint m_lastAnimTime{};
	GSCoord m_userPos{};
	std::jthread m_thread;
};

}
