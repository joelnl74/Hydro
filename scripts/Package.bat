@echo off
rem Makes a standalone build of a project: builds the Runtime project (Release or Dist), then copies what the
rem game needs into Builds\<Project>-<Kind>\. V1 copies the project's raw files: no cooking, no packing.
rem Oxygen's File > Build runs this script; it also works on its own (double-click: Release of the Sandbox).
rem
rem Usage:
rem   scripts\Package.bat [Release|Distribution] [options]
rem
rem Options:
rem   --project <file.hyproj>  the project to package (default Oxygen\project\Sandbox.hyproj; a relative path
rem                            is taken from the repository root)
rem   --no-build               package the Runtime that is already built
rem   --pause                  wait for a key at the end (default only when started by double-clicking)
rem   --no-pause               never wait for a key at the end
rem
rem What it makes (Builds\ is not in git):
rem   <Project>.exe            the Runtime, named after the project file
rem   shaderc_shared.dll       compiles the engine's shaders when the game starts
rem   assets\shaders\          the engine's shaders (Oxygen\assets\shaders)
rem   Project\<Project>.hyproj and Project\<asset folder>\   the project file and everything in its asset folder
rem   build-info.txt           what was built, from which commit, when
rem The game plays Project\<exe name>.hyproj, so the exe and the project file always share the name.
rem
rem Exit codes: 0 = packaged, 1 = the build failed, 2 = bad arguments, missing files or a failed copy.

setlocal EnableExtensions EnableDelayedExpansion

rem This script's folder, read before the arguments are parsed: shift also shifts %0, so %~dp0 is wrong after it.
set "SCRIPT_DIR=%~dp0"

set "KIND=Release"
set "PROJECT="
set "BUILD=1"

rem Pause at the end only when started from Explorer and not on a CI machine (see Build.bat).
set "PAUSE_AT_END=0"
set "LAUNCH_LINE=!cmdcmdline:"=!"
if /i not "!LAUNCH_LINE:%~f0=!"=="!LAUNCH_LINE!" if not defined CI set "PAUSE_AT_END=1"

:parse
if "%~1"=="" goto parsed
if /i "%~1"=="Release"      (set "KIND=Release"      & shift & goto parse)
if /i "%~1"=="Distribution" (set "KIND=Distribution" & shift & goto parse)
if /i "%~1"=="--no-build"   (set "BUILD=0"           & shift & goto parse)
if /i "%~1"=="--pause"      (set "PAUSE_AT_END=1"    & shift & goto parse)
if /i "%~1"=="--no-pause"   (set "PAUSE_AT_END=0"    & shift & goto parse)
if /i "%~1"=="--project" (
	if "%~2"=="" (echo [Package] --project needs a .hyproj file & set "EXIT_CODE=2" & goto finish)
	set "PROJECT=%~2"
	shift & shift & goto parse
)
echo [Package] Unknown argument: %~1
echo [Package] Usage: scripts\Package.bat [Release^|Distribution] [--project ^<file.hyproj^>] [--no-build] [--pause^|--no-pause]
set "EXIT_CODE=2"
goto finish
:parsed

if /i "%KIND%"=="Release" (set "CONFIG=Release") else (set "CONFIG=Dist")

pushd "%SCRIPT_DIR%.."
set "ROOT=%CD%"

rem ---- 1. The project -------------------------------------------------------------
if not defined PROJECT set "PROJECT=Oxygen\project\Sandbox.hyproj"
for %%P in ("%PROJECT%") do (
	set "PROJECT_FILE=%%~fP"
	set "PROJECT_DIR=%%~dpP"
	set "PROJECT_NAME=%%~nP"
)
if not exist "!PROJECT_FILE!" (
	echo [Package] Project not found: !PROJECT_FILE!
	set "EXIT_CODE=2" & goto finish_popd
)

rem The asset folder named in the project file (ProjectConfig::AssetDirectory), Assets when it isn't there.
set "ASSET_DIR=Assets"
for /f "tokens=1,* delims=:" %%a in ('findstr /r /c:"^ *AssetDirectory:" "!PROJECT_FILE!" 2^>nul') do (
	for /f "tokens=*" %%v in ("%%b") do set "ASSET_DIR=%%~v"
)
if not exist "!PROJECT_DIR!!ASSET_DIR!\" (
	echo [Package] The project's asset folder is missing: !PROJECT_DIR!!ASSET_DIR!
	set "EXIT_CODE=2" & goto finish_popd
)

set "OUT=%ROOT%\Builds\!PROJECT_NAME!-%KIND%"
set "BIN=%ROOT%\bin\%CONFIG%-windows-x86_64\Runtime"
echo [Package] !PROJECT_NAME! ^| %KIND% ^(%CONFIG%^) ^| !OUT!

rem ---- 2. Build the Runtime (and only what it links, so Oxygen can stay open) -------
if "%BUILD%"=="1" (
	call "%SCRIPT_DIR%Build.bat" %CONFIG% --project Runtime --no-pause
	if errorlevel 2 (set "EXIT_CODE=2" & goto finish_popd)
	if errorlevel 1 (set "EXIT_CODE=1" & goto finish_popd)
)
if not exist "!BIN!\Runtime.exe" (
	echo [Package] !BIN!\Runtime.exe not found. Build it first ^(leave out --no-build^).
	set "EXIT_CODE=2" & goto finish_popd
)

rem ---- 3. A clean output folder ------------------------------------------------------
if exist "!OUT!\" rmdir /s /q "!OUT!"
if exist "!OUT!\" (
	echo [Package] Could not empty !OUT! ^(is the game still running?^)
	set "EXIT_CODE=2" & goto finish_popd
)
mkdir "!OUT!\Project"

rem ---- 4. Copy: the exe as <Project>.exe, its DLL, the engine's shaders, the project -
echo [Package] Copying the game into !OUT!
copy /b /y "!BIN!\Runtime.exe" "!OUT!\!PROJECT_NAME!.exe" >nul || goto copy_failed
copy /b /y "!BIN!\shaderc_shared.dll" "!OUT!\shaderc_shared.dll" >nul || goto copy_failed
robocopy "%ROOT%\Oxygen\assets\shaders" "!OUT!\assets\shaders" /E /R:2 /W:1 /NFL /NDL /NJH /NJS /NP >nul
if errorlevel 8 goto copy_failed
copy /y "!PROJECT_FILE!" "!OUT!\Project\!PROJECT_NAME!.hyproj" >nul || goto copy_failed
rem Everything under the asset folder is copied: V1 knows nothing about individual assets.
robocopy "!PROJECT_DIR!!ASSET_DIR!" "!OUT!\Project\!ASSET_DIR!" /E /R:2 /W:1 /NFL /NDL /NJH /NJS /NP >nul
if errorlevel 8 goto copy_failed

rem ---- 5. build-info.txt ------------------------------------------------------------
set "COMMIT=unknown"
for /f %%c in ('git -C "%ROOT%" rev-parse --short HEAD 2^>nul') do set "COMMIT=%%c"
> "%OUT%\build-info.txt" (
	echo Project: !PROJECT_NAME!
	echo Kind: %KIND% ^(%CONFIG% configuration^)
	echo Commit: !COMMIT!
	echo Built: %DATE% %TIME%
	echo Source: !PROJECT_FILE!
)

echo [Package] DONE: !OUT!\!PROJECT_NAME!.exe
set "EXIT_CODE=0"
goto finish_popd

:copy_failed
echo [Package] FAILED: could not copy into !OUT!
set "EXIT_CODE=2"

:finish_popd
popd

:finish
if "%PAUSE_AT_END%"=="1" pause
exit /b %EXIT_CODE%
