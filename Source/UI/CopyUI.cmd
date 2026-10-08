@echo off
rem GSAliveAirport post-build helper (invoked by the PostBuildEvent).
rem Copies the UI assets next to the built exe.
rem Usage: CopyUI.cmd ^<OutDir^> [^<ignored^>]
if "%~1"=="" (
    echo Usage: CopyUI.cmd ^<OutDir^>
    exit /b 1
)
set "OutDir=%~1\"
copy /Y "%~dp0index.html" "%OutDir%" >nul || exit /b 1
copy /Y "%~dp0style.css" "%OutDir%" >nul || exit /b 1
copy /Y "%~dp0app.js" "%OutDir%" >nul || exit /b 1
exit /b 0
