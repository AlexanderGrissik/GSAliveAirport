# Animated Ground SimObjects

This document records animation capabilities discovered by inspecting the locally installed FSDT/GSX `sim.cfg` and model XML files.

The existence of an animation in the configuration is strong evidence that it can be controlled, but only the FSDT wing walker and marshaller have been verified through ParkingServices so far. Every additional model and animation carrier should be tested before it is used by the normal ground-service rules.

## Animation types

There are three separate kinds of movement:

1. **Spatial movement** moves the entire SimObject through the airport.
2. **Locomotion animation** moves a human model's limbs while the whole object is moved.
3. **Operational animation** deploys equipment such as platforms, belts, hoses, stairs, doors, and pushback mechanisms.

Simply moving an object does not necessarily activate its locomotion or operational animation. The relevant animation carrier SimVar and frame range must also be driven.

## Human models with locomotion or activity animations

| Model family | Available animation | Carrier or control |
|---|---|---|
| `FSDT_Marshaller_*` | Walking and full marshalling signals | `VELOCITY BODY Y` |
| `FSDT_Wingwalker_*` | Walking, attention, guiding, and operating | `VELOCITY BODY Y` |
| `FSDT_MAN1_STAIR_OPERATOR` | Walking and pushing equipment | `VELOCITY BODY Y` |
| `FSDT_Crew` / `FSDT_Attendant_*` | Walking, stooping, and greeting | `VELOCITY BODY Y` |
| `FSDT_Passengers` | Walking | `VELOCITY BODY Y` |
| `FSDT_Operator_Staircase_Aviramp` | Walking and staircase operation | `WAGON FRONT LINK ORIENTATION` |
| `FSDT_Fueler_Man1_Mercedes` | Walking, entering/exiting, hose, and pump operation | `WAGON BACK LINK ORIENTATION` |
| `FSDT_Baggage_Loader_Man_02` | Driving pose and complete loading/unloading sequence | `WAGON BACK LINK ORIENTATION` |
| `FSDT_Cleaning_Crew` | A walking animation is declared, but its frame map is less explicit | `WAGON FRONT LINK ORIENTATION` |

The easiest additional walking candidates for the current implementation are:

- `FSDT_MAN1_STAIR_OPERATOR`
- `FSDT_Attendant_hs1`
- FSDT passenger models

These use `VELOCITY BODY Y`, like the already verified wing walker and marshaller. They should fit the existing technique once their model-specific transition and walking-loop frames are configured.

`FSDT_catering_man_01` does not declare the same useful `[Couatl]` walking-animation map and is therefore a weaker standalone candidate.

## Catering truck finding

`FSDT_Catering_EU`, `FSDT_Catering_01`, and related FSDT catering trucks contain more than a movable truck model. Their configuration declares:

| Animation | Carrier | Configured range |
|---|---|---|
| Catering body arrival/raise | `PLANE TOUCHDOWN BANK DEGREES` | Arrival `0-550`; lift `550-1000` |
| Catwalk extension | `PLANE TOUCHDOWN NORMAL VELOCITY` | `0-100` |
| Embedded catering worker | `WAGON BACK LINK ORIENTATION` | Composite sequence `0-1000` |

The embedded worker sequence is configured as:

```text
anim_man_go = 0 500 30 500 530 3 530 1000 30
```

The truck therefore appears without an active worker when spawned at its default animation values, but the model has an embedded worker animation that should become visible when its carrier is advanced through the configured sequence.

This makes `FSDT_Catering_EU` one of the strongest candidates for a self-contained animated service: the truck body, catwalk, and catering worker can potentially be controlled without spawning a separate standalone worker.

## Baggage belt-loader finding

`FSDT_Tug_660` has a misleading name. Its GSX type is `BaggageLoader`, and its configuration declares a complete belt-loader animation set:

| Animation | Carrier | Configured range |
|---|---|---|
| Loader lift | `PLANE TOUCHDOWN NORMAL VELOCITY` | Up `0-300`; down `300-0` |
| Baggage loading/unloading sequence | `WAGON BACK LINK ORIENTATION` | Load `4213-645`; unload `645-4213` |
| Belt movement | `WAGON FRONT LINK ORIENTATION` | Loading loop `650-0`; unloading loop `0-650` |

The baggage sequence contains repeated grab/drop actions and a `wagonfull` action at its endpoint, indicating that baggage pieces are part of the baked animation.

`FSDT_Baggage_Loader_Man_02` provides the corresponding worker sequences:

```text
driver loop:          265-324
move into position:  324-645
unload:               645-4213
load:                 4213-645
return to driver:     0-265
```

The worker and belt loader intentionally share the important `645-4213` animation range. This strongly suggests that `FSDT_Tug_660` and `FSDT_Baggage_Loader_Man_02` can be spawned separately and synchronized by advancing their animation carriers together.

## Other equipment with operational animations

| Equipment family | Available actions |
|---|---|
| `FSDT_Cargo_Loader_CCL35S` | Platform lift and cargo guides |
| `FSDT_Cargo_Loader_CHAMP70*` | Platform, bridge, guide, bridge-width, and bridge-setup animations |
| `FSDT_Staircase_Aviramp` | Lift, stabilizers, and entry deployment |
| Other FSDT staircase families | Height/lift deployment |
| `FSDT_GPU_Hobart_4400` / `FSDT_GPU_TLD_406` | Power cable/hose deployment |
| `FSDT_Water_Truck` | Platform, hose, and embedded worker operations |
| `FSDT_Lavatory_Truck` | Platform, hose, and embedded worker operations |
| FSDT fuel trucks | Doors, platform, and hose deployment |
| `FSDT_DeIce_Safeaero_220` | Boom extension, elevation, and rotation |
| FSDT passenger buses | Door opening and closing |
| FSDT pushback tractors | Arms, clamps, wheel blocks, and towbar mechanisms |
| FSDT follow-me vehicles | Signs, flashers, and directional indicators |

### Cargo loaders

`FSDT_Cargo_Loader_CCL35S` exposes:

- Cargo guides through `WAGON FRONT LINK ORIENTATION`
- Platform height through `PLANE TOUCHDOWN NORMAL VELOCITY`

The `FSDT_Cargo_Loader_CHAMP70*` family additionally exposes:

- Bridge position through `WAGON FRONT LINK ORIENTATION`
- Guide position through `WAGON BACK LINK ORIENTATION`
- Bridge width through `PLANE TOUCHDOWN PITCH DEGREES`
- Bridge setup through `PLANE TOUCHDOWN HEADING DEGREES TRUE`

### Passenger stairs

The `FSDT_Staircase_Aviramp` family declares:

- Lift animation
- Stabilizers through `WAGON BACK LINK ORIENTATION`
- Entry opening/closing through `WAGON FRONT LINK ORIENTATION`

The separate `FSDT_Operator_Staircase_Aviramp` model contains walking, engine-start, stabilizer, stair, and entry-operation human sequences.

### Ground power units

The `FSDT_GPU_Hobart_4400` and `FSDT_GPU_TLD_406` families expose cable/hose deployment through `WAGON BACK LINK ORIENTATION`, using the range `0-100`.

### Water and lavatory trucks

Both truck families contain operational animations rather than being static vehicles:

- Platform movement
- Hose handling
- Embedded human operation

The water-truck worker also contains spray and control-button sequences. The lavatory-truck worker contains platform and control-button sequences.

### Fuel vehicles and workers

The FSDT Mercedes fuel-truck family declares:

- Platform lift
- Hose movement
- Left, right, truck, and platform doors

`FSDT_Fueler_Man1_Mercedes` contains the matching walking, vehicle-entry, pump, button, hose, and idle sequences.

### Deicing vehicles

`FSDT_DeIce_Safeaero_220` exposes:

- Boom extension/retraction
- Boom elevation
- Base rotation
- Boom-head rotation

### Pushback tractors

Depending on the model, FSDT pushbacks expose combinations of:

- Lifting and lowering arms
- Wheel clamps
- Wheel blocks
- Towbar and towbar-wheel mechanisms
- Driver-chair movement

### Passenger buses

The FSDT Cobus, Neoplan, passenger shuttle, and passenger-van families expose door-opening and door-closing animations.

## Objects without useful configured operation sequences

The following may still be moved spatially, but their GSX configurations do not declare useful service-operation animation channels:

- FSDT baggage wagons and cargo wagons
- FSDT ULDs and pallets
- FSDT passenger barriers
- Some older fuel-truck families
- The FSDT fire truck
- `FSDT_Van_Catering`

Vehicle model XML may still include standard wheel rotation or steering. That does not imply that a complete service sequence can be driven.

## Implementation implications

The current ParkingServices animation update writes `VELOCITY BODY Y`, which is sufficient for the verified wing walker, marshaller, and several promising walking-human families.

Equipment operation will require model-specific animation profiles capable of writing additional carrier SimVars, including:

```text
WAGON FRONT LINK ORIENTATION
WAGON BACK LINK ORIENTATION
PLANE TOUCHDOWN NORMAL VELOCITY
PLANE TOUCHDOWN BANK DEGREES
PLANE TOUCHDOWN PITCH DEGREES
PLANE TOUCHDOWN HEADING DEGREES TRUE
```

These are deliberately used by the installed FSDT models as animation carriers. They still need individual runtime verification through SimConnect before being treated as supported.

## Recommended experiment order

1. Drive the body, catwalk, and embedded worker of `FSDT_Catering_EU`.
2. Synchronize `FSDT_Tug_660` with `FSDT_Baggage_Loader_Man_02`.
3. Animate `FSDT_MAN1_STAIR_OPERATOR` with the existing walking technique.
4. Test GPU cable deployment.
5. Test staircase deployment and its separate operator.
6. Test water/lavatory embedded workers and hoses.

