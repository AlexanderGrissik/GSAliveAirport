# Building GSAliveAirport

## Prerequisites

| Requirement | Details |
|---|---|
| **OS** | Windows 10 or 11 (x64) |
| **Visual Studio** | 2026 (v18) Community or higher, with the **Desktop development with C++** workload |
| **C++ standard** | C++20 (`stdcpp20`) |
| **MSFS 2024 SDK** | SimConnect SDK (include, lib, dll). Default path: `D:\MSFS24\SDK`. Override with the `MSFS2024_SDK` MSBuild property. |
| **MSFS 2024** (to run) | Must be installed and running (or reachable) for the app to connect via SimConnect |
| **WebView2 Runtime** (to run) | Microsoft Edge WebView2 Runtime — built into Windows 11; on Windows 10 install it once from https://developer.microsoft.com/en-us/microsoft-edge/webview2/. The app starts its UI in WebView2 and exits if the runtime is missing. The per-user browser profile lives in `%LOCALAPPDATA%\GSAliveAirport` and is kept on disk across runs — it holds the WebView2 browser caches and the UI's persisted localStorage state (guarded by the stored `gsalive.version`). |
| **WebView2 SDK** (to build) | `Microsoft.Web.WebView2` NuGet package, version pinned in `GSAliveAirport.vcxproj`. Restored from nuget.org into the repo-local `Packages\` folder (git-ignored) — one-time network access on first build per machine/version. The loader is **statically linked**, so no WebView2 DLL is needed next to the exe. |

SDK folder layout expected under `$(MSFS2024_SDK)`:

```
SimConnect SDK/
  include/    (SimConnect.h etc.)
  lib/        (SimConnect.lib, SimConnect.dll)
```

## Build (Debug)

Open **PowerShell** in the project root and run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' .\GSAliveAirport.vcxproj '/t:Restore;Build' /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
```

`Restore` fetches the pinned WebView2 package into `Packages\` once (no-op when already present; Visual Studio does this automatically).

Output: `x64\Debug\GSAliveAirport.exe`

## Build (Release)

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' .\GSAliveAirport.vcxproj '/t:Restore;Build' /p:Configuration=Release /p:Platform=x64 /m /v:minimal
```

Output: `x64\Release\GSAliveAirport.exe`

## Single-file compile (check for errors in one `.cpp`)

Compiles only the specified source file without linking. Replace the path as needed.

```powershell
$log = & 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' .\GSAliveAirport.vcxproj '/t:Restore;ClCompile' /p:Configuration=Debug /p:Platform=x64 /p:SelectedFiles=Source\General\GSSimConnect.cpp /m /v:minimal 2>&1; $log | Select-String '\berror\b' | Select-Object -First 1
```
