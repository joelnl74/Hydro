@echo off
rem Starts the built Oxygen editor. It finds its own files next to the exe and opens the last project (or shows
rem the project hub), so the working directory doesn't matter. Build first with scripts\Build.bat.
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
start "Oxygen (%CONFIG%)" "%EXE%" %2 %3 %4 %5 %6 %7 %8 %9
exit /b 0
