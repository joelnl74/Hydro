@echo off
rem Starts the Oxygen editor the way Visual Studio does: working directory Oxygen\, so assets\ and
rem project\ are found. Build first with scripts\Build.bat.
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
cd /d "%~dp0..\Oxygen"
start "Oxygen (%CONFIG%)" "%EXE%"
exit /b 0
