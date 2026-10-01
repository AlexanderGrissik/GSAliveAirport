# Building GSLiveAirportMSFS

## Prerequisites

| Requirement | Details |
|---|---|
| **OS** | Windows 10 or 11 (x64) |
| **Visual Studio** | 2026 (v18) Community or higher, with the **Desktop development with C++** workload |
| **C++ standard** | C++20 (`stdcpp20`) |
| **MSFS 2024 SDK** | SimConnect SDK (include, lib, dll). Default path: `D:\MSFS24\SDK`. Override with the `MSFS2024_SDK` MSBuild property. |
| **MSFS 2024** (to run) | Must be installed and running (or reachable) for the app to connect via SimConnect |

SDK folder layout expected under `$(MSFS2024_SDK)`:

```
SimConnect SDK/
  include/    (SimConnect.h etc.)
  lib/        (SimConnect.lib, SimConnect.dll)
```

## Build (Debug)

Open **PowerShell** in the project root and run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' .\GSLiveAirportMSFS.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
```

Output: `x64\Debug\GSLiveAirportMSFS.exe`

## Build (Release)

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' .\GSLiveAirportMSFS.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m /v:minimal
```

Output: `x64\Release\GSLiveAirportMSFS.exe`

## Single-file compile (check for errors in one `.cpp`)

Compiles only the specified source file without linking. Replace the path as needed.

```powershell
$log = & 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' .\GSLiveAirportMSFS.vcxproj /t:ClCompile /p:Configuration=Debug /p:Platform=x64 /p:SelectedFiles=Source\General\GSSimConnect.cpp /m /v:minimal 2>&1; $log | Select-String '\berror\b' | Select-Object -First 1
```
