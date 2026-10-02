// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include <random>

namespace NS_GSAliveAirport
{

class GSRandom
{
public:
    inline static std::mt19937& Engine() {
        thread_local std::mt19937 engine{ std::random_device{}() };
        return engine;
    }

    inline static size_t RandSizeT(size_t from, size_t to) {
        std::uniform_int_distribution<std::size_t> distribution(from, to);
        return distribution(Engine());
    }
};

}