@echo off
rem GSAliveAirport post-build helper (invoked by the PostBuildEvent).
rem Copies the UI assets next to both Debug and Release executables.
for %%D in ("%~dp0..\..\x64\Debug" "%~dp0..\..\x64\Release") do (
    if not exist "%%~fD\" mkdir "%%~fD" || exit /b 1
    copy /Y "%~dp0index.html" "%%~fD\" >nul || exit /b 1
    copy /Y "%~dp0style.css" "%%~fD\" >nul || exit /b 1
    copy /Y "%~dp0app.js" "%%~fD\" >nul || exit /b 1
    copy /Y "%~dp0Logo.png" "%%~fD\" >nul || exit /b 1
)
exit /b 0
