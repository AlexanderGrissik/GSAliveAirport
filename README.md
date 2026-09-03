# ParkingServices probe

ParkingServices is an MSFS 2024 SimConnect console application that tracks
nearby non-user aircraft and automatically creates a small direct-control
ground-service test group for likely departure traffic. It also retains the
original diagnostic commands for requesting three stock simulator services:

- catering (`REQUEST_CATERING`)
- ground power (`REQUEST_POWER_SUPPLY`)
- baggage (`REQUEST_LUGGAGE`)

The application does not control aircraft, modify flight plans, request
pushback, or target the user aircraft.

## Tracking and automatic service policy

Every 10 seconds, `AircraftTrackerThread` requests aircraft within 5 km of the
user aircraft. A newly observed aircraft is admitted to the
tracked set only while it is within 1 km. Once admitted, it remains tracked
while it is still detected within 5 km. Leaving the 5 km scan or disappearing
from MSFS removes it from tracking and removes all of its created services.

A separate `GroundServicesThread` requests an in-memory copy of the tracked
aircraft every 10 seconds. This does not cause another SimConnect scan. Its
current `GroundServicesDecision` creates services when an aircraft is on the
ground, has its navigation light on, is moving below 1 knot, and is not in
`STATE_SIMPLE_TAXI`. Services are removed when its state becomes
`STATE_SIMPLE_TAXI`, its speed exceeds 2 knots, or the aircraft disappears.
Engine state is deliberately not part of this policy.

The automatic test group contains:

- `FSDT_Catering_EU`
- animated `FSDT_Wingwalker_Male_04`
- standing `Marshaller_Male_Summer_Caucasian`

Created objects are owned by their parent aircraft ObjectID, making service
creation idempotent and cleanup per-aircraft. Late creation responses are
immediately removed when their parent no longer qualifies.

The application also subscribes to MSFS `ObjectRemoved` notifications. If SI
or MSFS removes an aircraft, `AircraftTrackerThread` immediately evicts it and
`GroundServicesThread` cancels pending creates and removes every owned service
object. If MSFS removes one of the created ground objects, it is removed from
the service ownership maps and from `AnimationThread` immediately. The normal
5 km scan remains the fallback for an aircraft that disappears without a
notification.

The terminal runs on the main thread. Aircraft tracking, ground-service
decisions, animation, and SimConnect each have one owning class and one owned
`std::jthread`. All SimConnect requests and object mutations are queued to
`SimConnectThread`, so the SDK is never called concurrently.

## Architecture

The application is composed from thread-owning, state-owning components:

- `SimConnectThread` owns its `std::jthread`, the `SimConnectSession`, the SDK
  connection, packed wire formats, request correlation, pending scans and
  creates, and the coalesced animation-write queue.
  `SimConnectSession` is its low-level handle wrapper and is never called from
  another thread.
- `AircraftTrackerThread` owns its `std::jthread`, the automatic 10-second
  aircraft scan schedule, the latest complete 5 km view, 1 km/5 km tracking
  hysteresis, distances, and observation timestamps. Consumers receive copied
  snapshots under a mutex.
- `GroundServicesThread` owns its `std::jthread`, `GroundServicesDecision`,
  pending creates, created ObjectIDs, parent-aircraft relationships, native
  request cooldowns, and cleanup. Each decision cycle reads a copied tracker
  snapshot; it does not query MSFS itself.
- `AnimationThread` owns its `std::jthread`, animated ObjectIDs, routes, model
  frame ranges, and animation-probe CSV. It targets 30 updates per second for
  each worker. The initial safety cap is 20 simultaneously animated workers;
  excess created workers remain standing.
- `GroundServicesConfig` owns `SimObjectCatalog` and resolves service families
  from its raw, all-object catalog during serialized connection initialization.
- `ConsoleController` owns command parsing, selection, and terminal rendering.
- `ParkingServicesApp` runs on the main thread and only routes terminal
  commands, polls the one-shot `ground` future, and enforces shutdown order.

There is deliberately no ground-object polling thread or ground tracker.
`ground` requests one debugging snapshot through `SimConnectThread`; service
ownership is already known from successful create callbacks, and simulator
removal notifications clean it up.

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
3. Type `tracked`, `aircraft1`, or `aircraft5` to inspect traffic.
4. Request one service with `catering`, `gpu`, or `baggage`. The command is
   sent to every parked non-user aircraft in the latest complete 5 km snapshot.
5. Optionally use `select <ObjectID>` and `target` to inspect one aircraft.
6. Observe the simulator and use `ground` to list nearby ground SimObjects and
   their distance from the selected aircraft.

For repeatable testing, temporarily raise Airport Vehicle Density and Worker
Density in MSFS. The simulator may ignore a request when the aircraft model,
parking, airport services, or traffic injector does not support it.

## Commands

```text
status
tracked
aircraft1
aircraft5
aircraft
parked
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
reset
help
quit
```

- `tracked` lists every retained aircraft, including aircraft currently between
  1 km and 5 km that first entered tracking inside 1 km.
- `aircraft1` and `aircraft5` list the latest complete observations in those radii.
- `aircraft` displays the detailed latest-snapshot view.
- `ground` requests and prints one current 5 km ground-SimObject debug snapshot.
- `reset` clears the tracker and requests removal of every object created by
  the application. Automatic discovery resumes on the next periodic scan.

Scanning is automatic; the old manual `scan` command is no longer required.

`fsdt` directly creates a six-object test group beside every stationary,
grounded non-user aircraft:

- `FSDT_Catering_EU`
- `ASO_Baggage_Cart01`
- `FSDT_catering_man_01`
- `FSDT_Wingwalker_Male_04`
- `FSDT_Marshaller_01`
- `Marshaller_Male_Summer_Caucasian`

The FSDT objects require the corresponding FSDT/GSX SimObjects to be installed.
The stock Asobo marshaller remains standing because its animation graph cannot
be driven with the FSDT frame technique. `clearfsdt` removes all test objects
created by the probe.

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

The complete raw SimObject/livery catalog is enumerated once, synchronously,
after connecting to MSFS. It is retained by `GroundServicesConfig` only until
the session disconnects, and is used to resolve configured service families.

An aircraft is marked `eligible` only as a diagnostic hint when it has remained
on the ground below 1 knot for at least 10 seconds and exposes both an origin
and destination. Service commands do not require that marker; they target every
non-user aircraft that is on the ground and moving below 2 knots. Repeating the
same service command within five seconds skips aircraft protected by the
duplicate-request cooldown.
