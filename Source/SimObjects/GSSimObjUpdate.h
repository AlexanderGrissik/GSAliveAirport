// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
namespace NS_GSAliveAirport
{

class GSSimObj;

struct GSSimObjUpdate {
    virtual void OnSpawned(bool ok, GSSimObj& obj) = 0;
    virtual void OnDespawned() = 0;
};

}