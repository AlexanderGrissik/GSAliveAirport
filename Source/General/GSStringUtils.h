// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include <array>
#include <cctype>
#include <cstddef>
#include <string>

namespace NS_GSAliveAirport
{

class GSStringUtils
{
public:

    template <std::size_t N>
    static std::string ToLower(const std::array<char, N> &value);
};

// Stops at the first null character, or the array boundary if not terminated.
template <std::size_t N>
std::string GSStringUtils::ToLower(const std::array<char, N> &value)
{
    std::string result;
    result.reserve(N);
    for (unsigned char c : value) {
        if (c == '\0') break;
        result.push_back(static_cast<char>(std::tolower(c)));
    }
    return result;
}

} // namespace NS_GSAliveAirport
