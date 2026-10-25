// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSPassenger.h"
#include "../General/GSGeography.h"
#include "../General/GSCatalog.h"
#include "../General/GSRandom.h"
#include "../Animation/GSAnimMulti.h"
#include <cstddef>
#include <iterator>

namespace NS_GSAliveAirport
{

const std::unordered_map<std::string, std::array<GSAnimationObject::FrameRangeInfo, 3>> GSPassenger::s_ExtPassengerFrames = {
    { "FSDT_Passenger_BUSINESS_F_0008", { { {145.f, 423.f, 30.f}, {454.f, 485.f, 30.f}, {145.f, 423.f, 30.f} } } },
    { "FSDT_Passenger_BUSINESS_F_0024", { { {145.f, 423.f, 30.f}, {454.f, 485.f, 30.f}, {145.f, 423.f, 30.f} } } },
    { "FSDT_Passenger_BUSINESS_M_0004", { { {145.f, 423.f, 30.f}, {454.f, 485.f, 30.f}, {145.f, 423.f, 30.f} } } },
    { "FSDT_Passenger_BUSINESS_M_0027", { { {145.f, 423.f, 30.f}, {454.f, 485.f, 30.f}, {145.f, 423.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_F_0002",   { { {316.f, 522.f, 30.f}, {557.f, 590.f, 30.f}, {316.f, 522.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_F_0003",   { { {122.f, 451.f, 30.f}, {490.f, 526.f, 30.f}, {122.f, 451.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_F_0004",   { { {330.f, 630.f, 30.f}, {665.f, 697.f, 30.f}, {330.f, 630.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_F_0027",   { { {176.f, 470.f, 30.f}, {503.f, 534.f, 30.f}, {176.f, 470.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_F_0028",   { { {136.f, 432.f, 30.f}, {465.f, 498.f, 30.f}, {136.f, 432.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_F_0036",   { { {295.f, 576.f, 30.f}, {608.f, 640.f, 30.f}, {295.f, 576.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_F_0039",   { { {145.f, 423.f, 30.f}, {454.f, 485.f, 30.f}, {145.f, 423.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_M_0003",   { { {276.f, 617.f, 30.f}, {656.f, 693.f, 30.f}, {276.f, 617.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_M_0004",   { { {152.f, 488.f, 30.f}, {526.f, 563.f, 30.f}, {152.f, 488.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_M_0006",   { { {558.f, 915.f, 30.f}, {956.f, 995.f, 30.f}, {558.f, 915.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_M_0014",   { { {168.f, 369.f, 30.f}, {403.f, 436.f, 30.f}, {168.f, 369.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_M_0016",   { { {155.f, 472.f, 30.f}, {509.f, 544.f, 30.f}, {155.f, 472.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_M_0020",   { { {276.f, 617.f, 30.f}, {656.f, 694.f, 30.f}, {276.f, 617.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_M_0022",   { { {113.f, 340.f, 30.f}, {366.f, 391.f, 30.f}, {113.f, 340.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_M_0023",   { { {155.f, 472.f, 30.f}, {508.f, 544.f, 30.f}, {155.f, 472.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_M_0024",   { { {155.f, 472.f, 30.f}, {509.f, 545.f, 30.f}, {155.f, 472.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_M_0026",   { { {155.f, 472.f, 30.f}, {510.f, 545.f, 30.f}, {155.f, 472.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_M_0027",   { { {155.f, 472.f, 30.f}, {509.f, 544.f, 30.f}, {155.f, 472.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_M_0028",   { { {155.f, 472.f, 30.f}, {509.f, 544.f, 30.f}, {155.f, 472.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_M_0030",   { { {155.f, 472.f, 30.f}, {508.f, 543.f, 30.f}, {155.f, 472.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_M_0031",   { { {223.f, 582.f, 30.f}, {613.f, 643.f, 30.f}, {223.f, 582.f, 30.f} } } },
    { "FSDT_Passenger_CASUAL_M_0034",   { { {144.f, 423.f, 30.f}, {454.f, 485.f, 30.f}, {144.f, 423.f, 30.f} } } },
    { "FSDT_Passenger_CMan_0026",       { { {151.f, 562.f, 30.f}, {606.f, 651.f, 30.f}, {151.f, 562.f, 30.f} } } },
    { "FSDT_Passenger_CMan_0027",       { { {151.f, 562.f, 30.f}, {606.f, 651.f, 30.f}, {151.f, 562.f, 30.f} } } },
    { "FSDT_Passenger_CMan_0028",       { { {151.f, 562.f, 30.f}, {610.f, 656.f, 30.f}, {151.f, 562.f, 30.f} } } },
    { "FSDT_Passenger_PARTY_F0004",     { { {168.f, 517.f, 30.f}, {459.f, 496.f, 30.f}, {168.f, 517.f, 30.f} } } },
    { "FSDT_Passenger_PARTY_M0001",     { { {92.f, 421.f, 30.f}, {459.f, 496.f, 30.f}, {92.f, 421.f, 30.f} } } },
    { "FSDT_Passenger_RP_21",           { { {178.f, 391.f, 30.f}, {429.f, 464.f, 30.f}, {178.f, 391.f, 30.f} } } },
    { "FSDT_Passenger_RP_26",           { { {161.f, 529.f, 30.f}, {572.f, 613.f, 30.f}, {161.f, 529.f, 30.f} } } },
    { "FSDT_Passenger_RP_27",           { { {152.f, 399.f, 30.f}, {769.f, 851.f, 30.f}, {152.f, 399.f, 30.f} } } },
    { "FSDT_Passenger_RP_38",           { { {307.f, 663.f, 30.f}, {703.f, 742.f, 30.f}, {307.f, 663.f, 30.f} } } },
    { "FSDT_Passenger_RP_39",           { { {66.f, 177.f, 30.f}, {210.f, 246.f, 30.f}, {66.f, 177.f, 30.f} } } },
    { "FSDT_Passenger_RP_40",           { { {558.f, 915.f, 30.f}, {950.f, 990.f, 30.f}, {558.f, 915.f, 30.f} } } },
    { "FSDT_Passenger_RP_41",           { { {201.f, 566.f, 30.f}, {608.f, 729.f, 30.f}, {201.f, 566.f, 30.f} } } },
    { "FSDT_Passenger_RP_43",           { { {201.f, 566.f, 30.f}, {614.f, 654.f, 30.f}, {201.f, 566.f, 30.f} } } },
    { "FSDT_Passenger_RP_48",           { { {152.f, 488.f, 30.f}, {524.f, 561.f, 30.f}, {152.f, 488.f, 30.f} } } },
    { "FSDT_Passenger_RP_49",           { { {154.f, 518.f, 30.f}, {595.f, 676.f, 30.f}, {154.f, 518.f, 30.f} } } },
    { "FSDT_Passenger_RP_51",           { { {132.f, 499.f, 30.f}, {543.f, 583.f, 30.f}, {132.f, 499.f, 30.f} } } },
    { "FSDT_Passenger_RP_85",           { { {201.f, 566.f, 30.f}, {605.f, 645.f, 30.f}, {201.f, 566.f, 30.f} } } },
    { "FSDT_Passenger_RP_95",           { { {133.f, 379.f, 30.f}, {420.f, 461.f, 30.f}, {133.f, 379.f, 30.f} } } },
    { "FSDT_Passenger_RP_98",           { { {168.f, 517.f, 30.f}, {557.f, 596.f, 30.f}, {168.f, 517.f, 30.f} } } },
    { "FSDT_Passenger_RP_102",          { { {122.f, 451.f, 30.f}, {489.f, 525.f, 30.f}, {122.f, 451.f, 30.f} } } },
    { "FSDT_Passenger_RP_114",          { { {559.f, 915.f, 30.f}, {954.f, 993.f, 30.f}, {559.f, 915.f, 30.f} } } },
};

bool GSPassenger::OnCreated()
{
    Freeze();

    if (m_dynamic) {
        const auto& itr = s_ExtPassengerFrames.find(m_title);
        if (itr != s_ExtPassengerFrames.end()) {
            auto* anim = new GSAnimMulti(m_simObjectID, m_stairBasePos, true, GSDefinitions::GSDefID_AnimVelocBodyY);

            anim->AddFrameSet(itr->second, m_departing);
            anim->SetAIWaypoints(&m_aiWaypoints);
            RegisterAnim(anim);
        }
    }

    return true;
}

bool GSPassenger::PreSpawn()
{
    const auto& airData = m_aircraft.GetRawData();

    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    m_initPos.OnGround = 1;

    auto& catalog = GSCatalog::GetInstance();
    m_title = catalog.GetPassengerExt();
    if (m_title.empty()) {
        m_dynamic = false;
        m_title = catalog.GetTarmacHuman();
    } else if (!s_ExtPassengerFrames.contains(m_title)) {
        const auto index = GSRandom::RandSizeT(0, s_ExtPassengerFrames.size() - 1);
        m_title = std::next(s_ExtPassengerFrames.begin(), static_cast<std::ptrdiff_t>(index))->first;
    }

    auto halfWing = airData.wingSpanMeters / 5.0;
    double distX = GSRandom::RandDouble(-1.0, 1.0);
    double distY = GSRandom::RandDouble(halfWing, halfWing + 1.0);
    m_busPos = GSGeography::RelativePosition(m_stairBaseHeading, m_stairBasePos, distY, distX);

    if (m_departing) {
        m_initPos.Altitude = airData.groundAltitudeFeet;
        m_initPos.Longitude = m_busPos.Long();
        m_initPos.Latitude = m_busPos.Lat();
        m_initPos.Heading = GSGeography::Azz(m_busPos, m_stairBasePos);
    } else {
        m_initPos.Altitude = m_doorAltFeet;
        m_initPos.Longitude = m_doorPos.Long();
        m_initPos.Latitude = m_doorPos.Lat();
        m_initPos.Heading = m_stairBaseHeading;
    }

    if (m_dynamic) {
        BuildWalkPath();
    }
   
    return true;
}

void GSPassenger::BuildWalkPath()
{
    static unsigned flags = SIMCONNECT_WAYPOINT_ON_GROUND;

    const auto& initPos = GetInitPos();
    const GSCoord startPnt = { initPos.Longitude, initPos.Latitude };
    const float speedKTs = 2.2f;

    if (m_departing) {
        const float altGround = static_cast<float>(initPos.Altitude);
        AddWaypoint(m_busPos, altGround, speedKTs, 0.0, flags);
        AddWaypoint(m_stairBasePos, altGround, speedKTs, 0.0, flags);
        AddWaypoint(m_stairTopPos, m_doorAltFeet, speedKTs, 0.0, 0);
        AddWaypoint(m_doorPos, m_doorAltFeet, speedKTs, 0.0, 0);
    } else {
        const float altGround = m_aircraft.GetRawData().groundAltitudeFeet;
        AddWaypoint(m_doorPos, m_doorAltFeet, speedKTs, 0.0, 0);
        AddWaypoint(m_stairTopPos, m_doorAltFeet, speedKTs, 0.0, 0);
        AddWaypoint(m_stairBasePos, altGround, speedKTs, 0.0, 0);
        AddWaypoint(m_busPos, altGround, speedKTs, 0.0, flags);
    }
}

}
