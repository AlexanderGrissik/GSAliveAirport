# Ground Object Behaviour Templates

This document records how the **built-in / Asobo** ground-service objects (`ASO_*`, and the `Car Ground Power Unit` family) are animated. It is the complement to `ANIMATED_GROUND_OBJECTS.md`, which covers the FSDT `[Couatl]` carrier mechanism.

Findings are sourced from the official MSFS SDK documentation. As with the FSDT carriers, the documented control is strong evidence but **not yet verified through ParkingServices**; the object-scoped writes below must be tested at runtime before being relied on by the normal ground-service rules.

## Two animation mechanisms

| Object family | Animation mechanism | Control |
|---|---|---|
| FSDT (`FSDT_*`) | `[Couatl]` animation maps | Arbitrary carrier SimVars (`WAGON ... ORIENTATION`, `PLANE TOUCHDOWN ...`, `VELOCITY BODY Y`) |
| Built-in / Asobo (`ASO_*`, `Car Ground Power Unit`) | Asobo Model Behavior Templates | SDK "Services" SimVars — a `...TARGET` (write) and `...CURRENT` (read) pair |

Asobo behaviour templates (defined in `Asobo.xml`, the "Ground Vehicle ModelBehavior Helpers" package) **smoothly evolve a named model animation toward a target SimVar**, clamped to a configured range, at a configured speed. Each frame the object reads the `...TARGET` value and moves the animation toward it, updating the `...CURRENT` read-back. This is the SDK-native path: the object animates itself, and the plugin only needs to set the target value.

## Catering truck — `ASO_Catering_Truck_01`

Driven by two Asobo behaviour templates (per the CateringTruck Definition):

| Behaviour template | Model animation | Target (write) | Read-back | Range / speed |
|---|---|---|---|---|
| `ASOBO_CateringTruck_Elevation_Template` | `Elevation` (lift up/down) | `CATERINGTRUCK ELEVATION TARGET` (m) | `CATERINGTRUCK ELEVATION CURRENT` (m) | clamp `[BASE_HEIGHT, MAX_HEIGHT]`, `METERS_PER_SECOND` |
| `ASOBO_CateringTruck_Overture_Template` | `Deployment` (bridge / door open) | `CATERINGTRUCK OPENING TARGET` (0/1) | `CATERINGTRUCK OPENING CURRENT` (0/1) | template clamps internally, `PERCENT_PER_SECOND` |

Reference values from the SDK example: `BASE_HEIGHT ≈ 1.241 m`, `MAX_HEIGHT ≈ 5.35 m`, `METERS_PER_SECOND ≈ 0.2`. The overture (bridge) animation plays **only after** the container has reached its target elevation, and reverses again before the container is lowered.

Additional variable: `CATERINGTRUCK AIRCRAFT DOOR CONTACT OFFSET Z` (meters) — the bridge-end contact point height, reflected from the object's `sim.cfg`:

```ini
[GroundVehicle]
...
[CateringTruck]
aircraft_door_contact_offset_Z = <meters, required>
```

**Planned drive sequence:**

1. Spawn the object and capture its `SimObjectID`.
2. Write `CATERINGTRUCK ELEVATION TARGET` = the aircraft's pax-door sill height (meters AGL).
3. Write `CATERINGTRUCK OPENING TARGET` = 1 (deploy the bridge once elevation settles). SDK value is 0/1, like `FUELTRUCK HOSE DEPLOYED` and `GROUNDPOWERUNIT HOSE DEPLOYED`.
4. Read `CATERINGTRUCK ELEVATION CURRENT` / `...OPENING CURRENT` to confirm it has settled.
5. On removal: `OPENING TARGET` = 0, `ELEVATION TARGET` = low value, then remove the object.

The target elevation is exactly the value the pax-door scan in `GSReqAircraftScan.cpp` is meant to provide.

## Ground power unit — `Car Ground Power Unit`

The GPU cable is a **shared asset**: the truck exposes its own hose node, and the connecting cable to the receptacle is defined on the **aircraft model** (SDK: "a cable that is at least partly in the aircraft model, but hidden by default").

| Piece | Where it lives | How it is shown |
|---|---|---|
| GPU hose node | Ground vehicle | `ASOBO_GroundPowerUnit_Hose_Deployment_Template` (param `NODE_ID`) — hidden by default, visible when connected to an aircraft |
| Connecting cable | Aircraft model | `ASOBO_ET_COMMON_Interactive_Point_Visibility_Template` (param `NODE_TO_HIDE`) driven by the aircraft's **Ground Power Cable interactive point** |

The SDK's settable control (Services Variables, "Ground Power Units" section):

| Variable | Description (verbatim) | Units |
|---|---|---|
| **`GROUNDPOWERUNIT HOSE DEPLOYED`** | "The current deployment amount of the ground power unit hose. Currently can only be set to 0 (not deployed) and 1 (deployed)." | Percent over 100 |
| `GROUNDPOWERUNIT HOSE END POSX` | "The 'X' axis position of the end of the ground power unit hose when fully deployed, relative to the ground." | Meters |
| `GROUNDPOWERUNIT HOSE END POSZ` | "The 'Z' axis position of the end of the ground power unit hose when fully deployed, relative to the ground." | Meters |
| `GROUNDPOWERUNIT HOSE END RELATIVE HEADING` | "The heading of the end of the ground power unit hose, relative to the vehicle heading." | Degrees |

So the control is simply **`GROUNDPOWERUNIT HOSE DEPLOYED` = 1 (cable out) / 0 (stowed)**; the three `HOSE END …` variables describe the deployed hose-end geometry, reflected from the object's `sim.cfg`:

```ini
[GroundVehicle]
...
[GroundPowerUnit]
hose_end_position_XZ = <meters, required>
hose_end_relative_heading = <degrees>
```

**Caveats:**

- A specific aircraft only shows the full connecting cable if that airframe's model has the GPU cable mesh plus a Ground Power Cable interactive point configured (the default Airbus / A320 family do).
- Do not confuse the object-scope `GROUNDPOWERUNIT HOSE DEPLOYED` with the aircraft-scope electrical state `GROUND POWER UNIT CONNECTED` / `GROUND POWER UNIT AVAILABLE` (is the aircraft *receiving* external power).

## Object-scoped SimVars (implementation)

Unlike the FSDT carrier path (which drives user-scope carrier SimVars), these Asobo services SimVars are **object-scoped** — the behaviour templates read them with the `A:` prefix. They must be written/read **on the spawned object's `SimObjectID`**, not on the user aircraft. `SimConnectThread` already exposes the required primitives:

| Operation | `SimConnectThread` method | SimConnect call |
|---|---|---|
| Spawn object | `BeginCreateObject` | `SimConnect_AICreateSimulatedObject_EX1` |
| Write a service SimVar | `BeginSetObjectData` | `SimConnect_SetDataOnSimObject` |
| Read a service SimVar | `BeginRequestObjectData` | `SimConnect_RequestDataOnSimObject` |
| Remove object | `BeginRemoveObject` | `SimConnect_AIRemoveObject` |

The spawned `dwObjectID` is already captured in `GSReqCreateObject` from `SIMCONNECT_RECV_ASSIGNED_OBJECT_ID`.

## Verification status

- **Documented (SDK):** both control sets above are confirmed in the official SDK pages (see Sources). `GROUNDPOWERUNIT HOSE DEPLOYED` is explicitly stated as settable to 0/1. The catering `...TARGET` (write) / `...CURRENT` (read) split is inferred from the universal MSFS Services-Variables convention and from the Asobo templates reading `...TARGET` as their driving input.
- **Not yet verified (runtime):** that a client-side `SimConnect_SetDataOnSimObject` write of these object-simvars on a spawned AI object is honoured by the sim. This must be confirmed with a minimal spawn → set → read probe before the objects are wired into the normal ground-service rules — consistent with the repo's existing rule that every animation carrier is tested before use.

## Sources

- CateringTruck Definition — https://docs.flightsimulator.com/html/Content_Configuration/SimObjects/Ground_Vehicles/CateringTruck_Definition.htm
- GroundPowerUnit Definition — https://docs.flightsimulator.com/msfs2024/html/5_Content_Configuration/Modular_SimObjects/SimObjects/Ground_Vehicles/GroundPowerUnit_Definition.htm
- Services Variables (Ground Power Units / CateringTruck) — https://docs.flightsimulator.com/html/Programming_Tools/SimVars/Services_Variables.htm
- Fuel Truck And GPU Connections — https://docs.flightsimulator.com/msfs2024/html/3_Models_And_Textures/Modeling/Aircraft/Airframe/Fuel_Truck_And_GPU_Connections.htm
- Ground Vehicle ModelBehavior Helpers — `Asobo.xml` template package (referenced by the two Definition pages)

