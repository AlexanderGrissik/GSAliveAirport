# GSAliveAirport

**GSAliveAirport** is a Ground-Services companion application for **Microsoft Flight Simulator 2024**.

**The goal:** create an **alive airport atmosphere**.

It watches nearby parked and taxiing traffic and automatically spawns animated ground-service vehicles and crew — catering trucks, ground-power units, baggage loaders, baggage tugs, lavatory trucks, marshals, and wingmen — that drive along the airport's real road network to reach each aircraft.

## What it does

- **Tracks nearby aircraft** — continuously scans for non-user aircraft within range.
- **Spawns ground services** — when an aircraft is parked (arrived or preparing for departure), the app creates a matching set of ground-service vehicles and personnel next to it.
- **Moves services on the road network** — vehicles follow the airport's taxi/road paths to reach their target aircraft, just like real ground crews.
- **Cleans up automatically** — when an aircraft departs, leaves the area, or the sim closes, all associated services are removed.

The app is a **non-intrusive observer**: it does not control your aircraft, modify flight plans, or interfere with ATC. It only creates and removes decorative SimObjects around other traffic.

## Tested with

- **SayIntentions** injected traffic
- **GSX Pro** airport services

## Prerequisites

To **run** the released executable:

| Requirement | Details |
|---|---|
| **OS** | Windows 10 or 11 (64-bit) |
| **Microsoft Flight Simulator 2024** | Must be installed; the app connects to a running session via SimConnect |
| **Microsoft Visual C++ Redistributable (x64)** | Latest version. The app is built with the MSVC C++20 toolchain and depends on the standard `VCRUNTIME140.dll` / `MSVCP140.dll` system DLLs. Normally already present on Windows 10/11, but install the latest [VC++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist) if you get a missing-DLL error. |

> No other runtime dependencies. The app is a single self-contained `.exe`.

## How to run

1. Copy `GSAliveAirport.exe` **and** `SimConnect.dll` into the same folder.
   `SimConnect.dll` is part of the MSFS 2024 SDK and **must** sit next to the executable — the app loads it from the working directory at startup.
2. Make sure **MSFS 2024** is launched and you are in a session (on the ground or flying).
3. Run `GSAliveAirport.exe` from that folder (double-click or from a terminal).
4. The app prints a prompt. Type `help` for the list of console commands.

```
GSAliveAirport.exe        ← the application
SimConnect.dll            ← MSFS 2024 SimConnect runtime (same folder)
```

### Console commands

| Command | Description |
|---|---|
| `help` | Show all available commands |
| `tracked` | List all currently tracked aircraft |
| `tracked 1km` | List tracked aircraft within 1 km |
| `parked` | List tracked aircraft that are parked |
| `test` | Spawn a set of test aircraft near your position |
| `log` | Toggle detailed logging on/off |
| `debug` | Show or configure debug loggers |
| `quit` | Exit the application |

## Building from source

See [BUILD.md](BUILD.md) for build instructions, toolchain requirements, and MSBuild commands.

## License

See [LICENSE](LICENSE).
