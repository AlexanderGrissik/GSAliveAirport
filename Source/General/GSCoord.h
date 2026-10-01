// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

namespace NS_GSAliveAirport
{

class GSCoord
{
public:
    GSCoord() = default;
    GSCoord(double longitude, double latitude): m_longitude(longitude), m_latitude(latitude) {}

    GSCoord(const GSCoord&) = default;
    GSCoord& operator=(const GSCoord&) = default;

    bool operator==(const GSCoord& another) const
    {
        return m_longitude == another.m_longitude && m_latitude == another.m_latitude;
    }
    bool operator!=(const GSCoord& another) const { return !(*this == another); }

    double Long() const { return m_longitude; }
    double Lat() const { return m_latitude; }

private:
    double m_longitude{};
    double m_latitude{};
};

} // namespace NS_GSAliveAirport
