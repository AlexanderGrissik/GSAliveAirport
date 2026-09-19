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