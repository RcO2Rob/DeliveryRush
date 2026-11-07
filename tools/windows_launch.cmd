@echo off
setlocal
pushd "%~dp0.." || exit /b 1
where py >nul 2>nul
if not errorlevel 1 (
    py -3 -X utf8 tools\ue.py %1
) else (
    python -X utf8 tools\ue.py %1
)
set "launch_result=%errorlevel%"
if not "%launch_result%"=="0" (
    echo.
    echo Delivery Rush could not start. Read the message above.
    echo Setup instructions: Windows development guide in the project folder.
    pause
) else if "%~1"=="doctor" (
    pause
)
popd
exit /b %launch_result%
