# ParkingServices probe

Stage 1 is a read-mostly MSFS 2024 SimConnect console probe. It discovers nearby
AI aircraft and ground SimObjects and allows a user to manually request three
stock simulator services for all parked **non-user** aircraft in the current
snapshot:

- catering (`REQUEST_CATERING`)
- ground power (`REQUEST_POWER_SUPPLY`)
- baggage (`REQUEST_LUGGAGE`)

The probe does not automate service requests, control traffic, modify flight
plans, request pushback, or target the user aircraft.

Aircraft and ground SimObjects are scanned within 10 km of the user aircraft.

## Requirements

- Microsoft Flight Simulator 2024
- MSFS 2024 SDK 1.6.9 or compatible
- Visual Studio with the v145 C++ toolset and Windows 10/11 SDK
- x64 configuration

The project uses `MSFS2024_SDK` when that MSBuild property or environment
variable is set. Otherwise it defaults to `D:\MSFS24\SDK`.

The official static SimConnect library is used, so no SimConnect DLL needs to
be copied beside the executable.

## Running the probe

1. Start MSFS 2024 and load into a flight near an airport with AI traffic.
2. Run `ParkingServices.exe`.
3. Type `aircraft` to inspect nearby non-user aircraft.
4. Request one service with `catering`, `gpu`, or `baggage`. The command is
   sent to every parked non-user aircraft in the current 10 km snapshot.
5. Optionally use `select <ObjectID>` and `target` to inspect one aircraft.
6. Observe the simulator and use `ground` to list nearby ground SimObjects and
   their distance from the selected aircraft.

For repeatable testing, temporarily raise Airport Vehicle Density and Worker
Density in MSFS. The simulator may ignore a request when the aircraft model,
parking, airport services, or traffic injector does not support it.

## Commands

```text
status
aircraft
ground
select <ObjectID>
target
catering
gpu
baggage
fsdt
clearfsdt
animprobe <ObjectID>
stopprobe
scan
catalog
help
quit
```

`fsdt` directly creates a six-object test group beside every stationary,
grounded non-user aircraft:

- `FSDT_Catering_EU`
- `ASO_Baggage_Cart01`
- `FSDT_catering_man_01`
- `FSDT_Wingwalker_Male_04`
- `FSDT_Marshaller_01`
- `Marshaller_Male_Summer_Caucasian`

The FSDT objects require the corresponding FSDT/GSX SimObjects to be installed.
The stock Asobo marshaller is retained as a waypoint-only control and therefore
slides instead of playing a walking animation. `clearfsdt` removes all test
objects created by the probe.

## Animated FSDT worker control

The successful worker experiment reproduces the important parts of GSX's
external control technique. A worker is created with
`SimConnect_AICreateSimulatedObject_EX1`, but is not moved with an `AI Waypoint
List`. Standard waypoint movement changes the worker's position while leaving
the FSDT human model in its idle pose, producing a sliding worker.

FSDT human models contain baked animation timelines. Their model behavior reads
`VELOCITY BODY Y` as an animation-frame value. For the verified
`FSDT_Wingwalker_Male_04` model, the installed `sim.cfg` defines:

```text
idle-to-walk transition: 192-229 at 30 fps
walking loop:            230-267 at 30 fps
walk-to-idle transition: 270-307 at 30 fps
idle loop:               310-357 at 30 fps
```

The probe currently drives frames `192-229`, then continuously loops `230-267`.
At the same time it moves the worker around a four-point closed route by writing
these SimVars directly at approximately 30 updates per second:

```text
PLANE LATITUDE
PLANE LONGITUDE
PLANE ALTITUDE
PLANE HEADING DEGREES TRUE
VELOCITY BODY Y
```

Before direct movement begins, the probe sends these events to that worker with
a value of `1`:

```text
FREEZE_LATITUDE_LONGITUDE_SET
FREEZE_ALTITUDE_SET
FREEZE_ATTITUDE_SET
```

Freezing is essential. `VELOCITY BODY Y` is both the FSDT model's animation
carrier and a real simulator velocity variable. Without freezing, MSFS applies
physics to values in the animation-frame range, repeatedly launches the worker,
and alternates its `SIM ON GROUND` state. A recorded unfrozen test oscillated
between roughly 144 and 180 feet while the walking frame itself remained
correctly within `230-267`. With the three freezes active, direct movement and
the walking animation remain stable on the ground.

Repeatedly setting the compound `Initial Position` value was also unsuitable:
its `OnGround` field retriggered MSFS ground placement and caused vertical
snapping. The working implementation changes only latitude, longitude,
altitude, and heading after creation. Worker route altitude comes from the
nearby aircraft's `GROUND ALTITUDE`, rather than the aircraft reference point's
`PLANE ALTITUDE`.

In short, the verified sequence is:

1. Create the FSDT human SimObject.
2. Freeze latitude/longitude, altitude, and attitude physics for that ObjectID.
3. Advance the baked animation frame through `VELOCITY BODY Y`.
4. Write the desired world position and heading directly.
5. Repeat the animation and position updates at about 30 Hz.

This method is model-specific: animation ranges must be read from the selected
FSDT model's configuration and cannot be assumed to match another worker.

The `animprobe <ObjectID>` command records the selected object's position,
velocity, heading, ground state, and animation carrier continuously to
`animation_probe.csv` beside the executable. Recording continues until
`stopprobe` is entered. This was used to derive and verify the control method.

`catalog` asynchronously enumerates the complete installed spawnable catalog
and each SimObject category. It writes `simobject_catalog.txt` beside the
running executable, excluding aircraft, helicopters, hot-air balloons, boats,
and animals. Every retained row includes its SimConnect type; entries that do
not appear in a specific category are retained as `UNKNOWN`.

An aircraft is marked `eligible` only as a diagnostic hint when it has remained
on the ground below 1 knot for at least 10 seconds and exposes both an origin
and destination. Service commands do not require that marker; they target every
non-user aircraft that is on the ground and moving below 2 knots. Repeating the
same service command within five seconds skips aircraft protected by the
duplicate-request cooldown.
