@echo off
rem Starts this workspace's Oxygen (built with Eonlore's code) on Eonlore's project. Build first with
rem scripts\Build.bat.
rem
rem Usage: scripts\RunOxygen.bat [Debug|Release|Dist]   (default Debug)

setlocal
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Debug"
set "EXE=%~dp0..\bin\%CONFIG%-windows-x86_64\Oxygen\Oxygen.exe"
if not exist "%EXE%" (
	echo [RunOxygen] %EXE% not found. Build it first: scripts\Build.bat %CONFIG%
	pause
	exit /b 2
)
start "Eonlore - Oxygen (%CONFIG%)" "%EXE%" "%~dp0..\Project\Eonlore.hyproj" %2 %3 %4 %5 %6 %7 %8 %9
exit /b 0
