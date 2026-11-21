# GSAliveAirport

**GSAliveAirport** is a Ground-Services companion application for **Microsoft Flight Simulator 2024**.

**The goal:** create an **alive airport atmosphere**.

It watches nearby parked and taxiing traffic and automatically spawns animated ground-service vehicles and crew — catering trucks, ground-power units, baggage loaders, baggage tugs, lavatory trucks, marshals, and wingmen — that drive along the airport's real road network to reach each aircraft.

## What it does

- **Tracks nearby aircraft** — continuously scans for non-user aircraft within range.
- **Spawns ground services** — when an aircraft is parked (arrived or preparing for departure), the app creates a matching set of ground-service vehicles and personnel next to it.
- **Moves services on the road network** — vehicles follow the airport's taxi/road paths to reach their target aircraft, just like real ground crews.
- **Adds airport utility traffic** — utility vehicles circulate on service roads at the airport detected near your aircraft, where a suitable road network is available.
- **Cleans up automatically** — when an aircraft departs, leaves the area, or the sim closes, all associated services are removed.

The app is a **non-intrusive observer**: it does not control your aircraft, modify flight plans, or interfere with ATC. It only creates and removes decorative SimObjects around other traffic.

## Tested with

- **MSFS24** injected parking traffic
- **SayIntentions** injected parking traffic
- **GSX Pro** airport services

## Prerequisites

| Requirement | Details |
|---|---|
| **OS** | Windows 10 or 11 (64-bit) |
| **Microsoft Flight Simulator 2024** | Must be installed; the app connects to a running session via SimConnect |
| **Microsoft Visual C++ Redistributable (x64)** | Provides the runtime DLLs the application needs. Install it if Windows reports that `VCRUNTIME140.dll` or `MSVCP140.dll` is missing. [VC++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist) |
| **Microsoft Edge WebView2 Runtime** | Required for the desktop interface. See [BUILD.md](BUILD.md#prerequisites) for installation details. |

## How to run

1. Extract `GSAliveAirport.exe` and `SimConnect.dll` from the release archive into the same folder.
2. Open `GSAliveAirport.exe` and load a flight in Microsoft Flight Simulator 2024. The app connects automatically and detects your aircraft, the nearest airport, and nearby traffic.
3. In **Settings**, select the injector mode for your traffic source. The available modes are **SayIntentions**, **MSFS24 Default**, and **Any**.

### Desktop interface

| Page | What it shows |
|---|---|
| **Aircrafts** | Your aircraft, nearby tracked aircraft, and their ground-service status. |
| **Log** | Connection messages and application activity. |
| **Debug** | Controls to spawn test aircraft, toggle logging, and configure debug loggers. |
| **Settings** | Traffic injector options. Settings are saved between sessions. |

## Building from source

See [BUILD.md](BUILD.md) for build instructions, toolchain requirements, and MSBuild commands.

## License

See [LICENSE](LICENSE).
