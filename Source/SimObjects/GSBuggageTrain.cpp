// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSBuggageTrain.h"
#include "../General/GSGeography.h"
#include "../General/GSCatalog.h"
#include "GSStatic.h"

namespace NS_GSLiveAirportMSFS
{

bool GSBuggageTrain::OnCreated()
{
    Freeze();
    return true;
}

bool GSBuggageTrain::PreSpawn()
{
    const auto& loaderPos = m_loader.GetInitPos();

    m_initPos.Heading = GSGeography::NormDeg(loaderPos.Heading + 90.0);
    m_initPos.Airspeed = 0;
    m_initPos.Bank = 0.0;
    m_initPos.Pitch = 0.0;
    m_initPos.OnGround = 1;
    m_initPos.Altitude = loaderPos.Altitude;

    bool driver = true;
    m_title = GSCatalog::GetInstance().GetTugExt();
    if (m_title.empty()) {
        m_title = "ASO_Tug01_White";
        driver = false;
    }
    
    const auto tugPos = GSGeography::RelativePosition(
        loaderPos.Heading, { loaderPos.Longitude, loaderPos.Latitude }, -6, 5);
    m_initPos.Longitude = tugPos.Long();
    m_initPos.Latitude = tugPos.Lat();

    if (driver) {
        const auto driverPos = GSGeography::RelativePosition(
            m_initPos.Heading, { m_initPos.Longitude, m_initPos.Latitude }, -0.65318, -0.35635);

        auto ptrDriver = new GSStatic(m_simHandle, m_aircraft, *this);
        ptrDriver->SetTitle("FSDT_Driver_01_Tug_M1A");
        ptrDriver->SetPosition({ driverPos.Long(), driverPos.Lat() });
        ptrDriver->SetPosRel(GSStatic::REL_ABSOLUTE);
        ptrDriver->SetHeading(m_initPos.Heading);
        ptrDriver->GetInitPos().Altitude = 1.0961 + m_initPos.Altitude;
        ptrDriver->GetInitPos().OnGround = 0;
        ptrDriver->PreSpawn();
        m_attached.emplace_back(ptrDriver);
    }

    auto ptrCart1 = new GSStatic(m_simHandle, m_aircraft, *this);
    const auto cart1Pos = GSGeography::RelativePosition(
        m_initPos.Heading, { m_initPos.Longitude, m_initPos.Latitude }, -3, 0);
    ptrCart1->SetTitle("ASO_Baggage_Cart02");
    ptrCart1->GetInitPos() = m_initPos;
    ptrCart1->SetPosRel(GSStatic::REL_ABSOLUTE);
    ptrCart1->SetHeading(m_initPos.Heading);
    ptrCart1->SetPosition(cart1Pos);
    ptrCart1->PreSpawn();
    m_attached.emplace_back(ptrCart1);

    auto ptrCart2 = new GSStatic(m_simHandle, m_aircraft, *this);
    auto cart1PosInit = ptrCart1->GetInitPos();
    const auto cart2Pos = GSGeography::RelativePosition(
        cart1PosInit.Heading, { cart1PosInit.Longitude, cart1PosInit.Latitude }, -2.75, 0);
    ptrCart2->SetTitle("ASO_Baggage_Cart01");
    ptrCart2->GetInitPos() = cart1PosInit;
    ptrCart2->SetPosRel(GSStatic::REL_ABSOLUTE);
    ptrCart2->SetHeading(cart1PosInit.Heading);
    ptrCart2->SetPosition(cart2Pos);
    ptrCart2->PreSpawn();
    m_attached.emplace_back(ptrCart2);

    auto ptrCart3 = new GSStatic(m_simHandle, m_aircraft, *this);
    auto cart2PosInit = ptrCart2->GetInitPos();
    const auto cart3Pos = GSGeography::RelativePosition(
        cart2PosInit.Heading, { cart2PosInit.Longitude, cart2PosInit.Latitude }, -2.75, 0);
    ptrCart3->SetTitle("ASO_Baggage_Cart01");
    ptrCart3->GetInitPos() = cart2PosInit;
    ptrCart3->SetPosRel(GSStatic::REL_ABSOLUTE);
    ptrCart3->SetHeading(cart2PosInit.Heading);
    ptrCart3->SetPosition(cart3Pos);
    ptrCart3->PreSpawn();
    m_attached.emplace_back(ptrCart3);

    return true;
}

}
