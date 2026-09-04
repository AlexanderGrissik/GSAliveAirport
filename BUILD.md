# Building ParkingServices

Open PowerShell in this directory and run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' .\ParkingServices.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
```

& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' .\ParkingServices.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m /v:minimal

The executable is written to:

```text
x64\Debug\ParkingServices.exe
```

For a Release build, change `Configuration=Debug` to `Configuration=Release`.
