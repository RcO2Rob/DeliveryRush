@echo off
call "%~dp0tools\windows_launch.cmd" doctor
exit /b %errorlevel%
