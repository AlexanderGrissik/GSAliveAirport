# ParkingServices — First Compilation Plan

Goal: get the project to a clean build after the "Refactor 2" cleanup.
We work through each step together; each step is reviewed and approved before the next.

---

## Step 1 — Clean up the vcxproj ✅

- [x] Remove all ClCompile / ClInclude entries that reference files moved to `OldCode/` or deleted.
- [x] Remove the deleted `main.cpp` and `GSCommon.cpp` from the **active** (non-excluded) list.
- [x] Add `GSLogStream.cpp` as an active ClCompile.
- [x] `SimObjectPositioning.cpp` was deleted during refactor (no longer needed).
- [x] Add `GSLogStream.h`, `GSCommand.h`, `GSQuickQueue.h` to ClInclude.

## Step 2 — Fix `GSQuickQueue.h` ✅

- [x] Add `#pragma once`.
- [x] Add missing includes: `<atomic>`, `<array>`, `<cstddef>`, `<chrono>`, `<thread>`, `<type_traits>`, `<optional>`.
- [x] Fix typos: `test_And_set`, `memory_order_aquire`, `static_Cast`, `~t()`, `return std::nullptr`.
- [x] Remove duplicate `static_assert`.
- [x] Add missing closing brace `}` for the class.
- [x] Fix `pushPos + 1` bug in `Push()`.

## Step 3 — Fix `GSCommand.h` ✅

- [x] Add `#pragma once`.
- [x] Add missing includes: `<functional>` (for `reference_wrapper`), `<optional>`.
- [x] Change `typedef GSCmdQueue = ...` to `using GSCmdQueue = ...`.
- [x] Fix `is_set()` → `has_value()`.
- [x] Add missing semicolon after the class closing brace.

## Step 4 — Fix `GSLogStream.h` / `GSLogStream.cpp` ✅

- [] **`.cpp` line 1:** change `#include "GSCommon.h"` → `#include "GSLogStream.h"`.
- [] **`.h`:** add missing includes: `<array>`, `<cstddef>`.
- [] Verify all public functions (`GSSetLoggingEnabled`, `GSLoggingEnabled`, `GSPrint`, `Lower`) are declared and defined consistently.

## Step 5 — Fix `GSLiveAirportMSFSApp.h` / `GSLiveAirportMSFSApp.cpp` ✅

### Header
- [] Replace includes: `GSAircraftTrackerThread.h`, `GSConsole.h`, `GSSimConnectThread.h`.
- [] Remove `AnimationThread.h`, `GroundServicesThread.h`, `GroundServicesConfig.h`.
- [] Update member types: `GSAircraftTrackerThread`, `GSConsole`.
- [] Remove `m_animation`, `m_groundServices`.
- [] Remove dead `RunParkingServices()`.

### Source
- [] Replace `#include "GSCommon.h"` → `#include "GSLogStream.h"`.
- [] Replace all `ConsoleController::` with `GSConsole::`.
- [] Replace `AppCommandType` / `AppCommand` with `GSConsole::AppCommandType` / `GSConsole::AppCommand`.
- [] Fix `waitsForResult` — removed (was undefined).
- [] Remove references to `m_groundServices`, `m_animation`.

## Step 6 — Fix `GSAircraft.h` / `GSAircraft.cpp`

### Header
- Move `g_InteractivePointProbeCount` into the header (or a shared constants header) so it is visible where `AircraftWireData` uses it.
- Add `#include "SimConnectHandler.h"` (or forward-declare `SimConnectHandler`).
- Add SimConnect / Windows types include (via `SimConnectHandler.h` or `GSDefinitions.h`).
- Add `void Print();` declaration (used in the .cpp but not declared in the header).

### Source
- Line 60: `rc &=InvokeAddDatum(...)` → `rc &= handler.InvokeAddDatum(...)`.
- Line 64: add missing semicolon after `GSLogError(...)`.
- Verify `Print()` references are consistent (`ac` vs `*this`, `r` vs `m_rawData`, `km` variable, etc.).
- Add `#include "GSLogStream.h"` for `GSLog()` / `GSLogError()`.

## Step 7 — Fix `GSAircraftTrackerThread.h` / `GSAircraftTrackerThread.cpp`

### Header
- Remove `#include "GSRequests/GSReqAircraftScan.h"` and `"GSRequests/GSReqCommand.h"` (moved to OldCode).
- Fix class-name mismatches: destructor `~AircraftTrackerThread()` → `~GSAircraftTrackerThread()`.
- Add `#include "GSCommand.h"` (for `CMD_*` constants or `GSCommand`).
- Verify `m_nearby` member — either declare it or remove references to it.

### Source
- Replace all `AircraftTrackerThread::` method prefixes with `GSAircraftTrackerThread::`.
- Add `using namespace std::chrono_literals;` (or qualify `1000ms`, `50ms`).
- Line 87: `DOWRD` → `DWORD`; `NextRequestId` → `NextRequestID`.
- Lines 98-102: `messageSize` is not a parameter of `OnException` — remove or use `sizeof`.
- `HandleExistingAircraft`: reconcile the 2-param definition with the 1-param declaration.
- `HandleNewAircraft`: reconcile the `shared_ptr` definition with the `SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE&` declaration.
- Line 161: `m_tracked.emplace(aircraft)` → `m_tracked.emplace(aircraft->objectID, aircraft)`.
- Line 168: `szie` → `size`.
- Line 171: `aircraft->objectID` → `aircraft.second->objectID` (map iterator).
- Add `#include "SimObjectPositioning.h"` for `DistanceMeters`.
- Line 236: fix `r.latitude` → `ac.m_rawData.latitude` (or similar).
- `isParked` lambda: the body is empty — implement the parked-check logic.
- Add `#include "GSLogStream.h"`.

## Step 8 — Fix `SimConnectThread.h` / `SimConnectThread.cpp`

### Source
- `Connect()`: remove `stopToken` usage (it's not a parameter) or change the signature to accept one.
- Line 79: `OnSimEnd()` → `OnSimStop()`.
- Line 86: add missing semicolon after `OnException(...)`.
- Line 104: `.ok` → `.isOK()`.
- Add `#include "GSDefinitions.h"` for `GSEventID_SimState`.
- Add `#include "GSLogStream.h"` for `GSLog()`.
- Add `using namespace std::chrono_literals;` for `2000ms`.
- Verify `InvokeAddDatum` override returns `bool` (consistent with base class).

## Step 9 — Fix `GSConsole.h` / `GSConsole.cpp`

### Header
- Add `#include <functional>` for `std::function`.
- Fix constructor: `explicit ConsoleController() {}` → `GSConsole() = default;`.
- Fix copy-delete declarations: `ConsoleController` → `GSConsole`.
- Consider making `AppCommandType` and `AppCommand` accessible (they're nested; callers use them unqualified).

### Source
- Replace all `ConsoleController::` method prefixes with `GSConsole::`.
- Add `#include <functional>` if not pulled in via the header.

## Step 10 — First build attempt

- Run `msbuild ParkingServices.vcxproj /p:Configuration=Debug /p:Platform=x64 /t:Rebuild`
- Triage remaining errors, fix iteratively.

---

## Files NOT to touch

- `OldCode/` — reference only, not part of the build.
- `external/nlohmann-json/` — third-party, do not modify.

## Conventions

- Namespace: `parking_services`
- C++ standard: C++20
- Logging: use `GSLog()` / `GSLogError()` from `GSLogStream.h`
- Thread-safe command queue: `GSQuickQueue<T, CAP>` via `GSCommand.h`
