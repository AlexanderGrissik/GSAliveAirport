// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include <random>

namespace NS_GSLiveAirportMSFS
{

class GSRandom
{
public:
    inline static std::mt19937& Engine() {
        thread_local std::mt19937 engine{ std::random_device{}() };
        return engine;
    }
};

}