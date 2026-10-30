@echo off
rem Builds Hydro and Oxygen from the command line, without opening Visual Studio.
rem   1. regenerates the Visual Studio solution with premake (same as Win-GenProjects.bat)
rem   2. finds MSBuild through vswhere (or takes the one on PATH in a Developer Command Prompt)
rem   3. builds Hydro.sln and writes full, errors-only and warnings-only logs to bin\logs\
rem Meant for people, agents and CI alike: it never asks anything, and the exit code says what happened.
rem
rem Usage:
rem   scripts\Build.bat [Debug|Release|Dist] [options]
rem
rem Options:
rem   --project <name>  build one project of the solution (Hydro, Game, Oxygen or Runtime) instead of all of it
rem   --rebuild         rebuild instead of an incremental build
rem   --no-generate     skip the premake step (use the existing Hydro.sln)
rem   --action <name>   premake action, default vs2022 (also HYDRO_PREMAKE_ACTION)
rem   --pause           wait for a key at the end (default only when started by double-clicking)
rem   --no-pause        never wait for a key at the end
rem
rem Exit codes: 0 = build succeeded, 1 = build failed, 2 = bad arguments or missing prerequisites.
rem Logs: bin\logs\Build-<Config>.log, Build-<Config>-errors.log, Build-<Config>-warnings.log

setlocal EnableExtensions EnableDelayedExpansion

rem This script's folder, read before the arguments are parsed: shift also shifts %0, so %~dp0 is wrong after it.
set "SCRIPT_DIR=%~dp0"

set "CONFIG=Debug"
set "GENERATE=1"
set "TARGET=Build"
set "PROJECT="
set "ACTION=vs2022"
if defined HYDRO_PREMAKE_ACTION set "ACTION=%HYDRO_PREMAKE_ACTION%"

rem Pause at the end only when started from Explorer (the command line then holds this script's
rem full path) and not on a CI machine; --pause and --no-pause override that.
set "PAUSE_AT_END=0"
set "LAUNCH_LINE=!cmdcmdline:"=!"
if /i not "!LAUNCH_LINE:%~f0=!"=="!LAUNCH_LINE!" if not defined CI set "PAUSE_AT_END=1"

:parse
if "%~1"=="" goto parsed
if /i "%~1"=="Debug"         (set "CONFIG=Debug"   & shift & goto parse)
if /i "%~1"=="Release"       (set "CONFIG=Release" & shift & goto parse)
if /i "%~1"=="Dist"          (set "CONFIG=Dist"    & shift & goto parse)
if /i "%~1"=="--rebuild"     (set "TARGET=Rebuild" & shift & goto parse)
if /i "%~1"=="--no-generate" (set "GENERATE=0"     & shift & goto parse)
if /i "%~1"=="--pause"       (set "PAUSE_AT_END=1" & shift & goto parse)
if /i "%~1"=="--no-pause"    (set "PAUSE_AT_END=0" & shift & goto parse)
if /i "%~1"=="--project" (
	if "%~2"=="" (echo [Build] --project needs a project name, e.g. --project Oxygen & set "EXIT_CODE=2" & goto finish)
	set "PROJECT=%~2"
	shift & shift & goto parse
)
if /i "%~1"=="--action" (
	if "%~2"=="" (echo [Build] --action needs a premake action, e.g. --action vs2022 & set "EXIT_CODE=2" & goto finish)
	set "ACTION=%~2"
	shift & shift & goto parse
)
echo [Build] Unknown argument: %~1
echo [Build] Usage: scripts\Build.bat [Debug^|Release^|Dist] [--project ^<name^>] [--rebuild] [--no-generate] [--action ^<name^>] [--pause^|--no-pause]
set "EXIT_CODE=2"
goto finish
:parsed

pushd "%SCRIPT_DIR%.."
set "ROOT=%CD%"
set "LOGDIR=bin\logs"
if not exist "%LOGDIR%" mkdir "%LOGDIR%"
set "LOG=%LOGDIR%\Build-%CONFIG%.log"
set "ERRLOG=%LOGDIR%\Build-%CONFIG%-errors.log"
set "WARNLOG=%LOGDIR%\Build-%CONFIG%-warnings.log"

rem ---- Prerequisites ------------------------------------------------------------
if not defined VULKAN_SDK (
	echo [Build] VULKAN_SDK is not set. Install the Vulkan SDK ^(scripts\Setup.bat^) and open a new terminal.
	set "EXIT_CODE=2" & goto finish_popd
)
if not exist "%VULKAN_SDK%\Include\vulkan\vulkan.h" (
	echo [Build] VULKAN_SDK points to "%VULKAN_SDK%", which has no Include\vulkan\vulkan.h.
	set "EXIT_CODE=2" & goto finish_popd
)
rem Oxygen's pre-build step runs the reflection generator through the Python launcher.
where py >nul 2>nul
if errorlevel 1 (
	echo [Build] The Python launcher 'py' is not on PATH; Oxygen's pre-build step ^(reflection generator^) needs it.
	set "EXIT_CODE=2" & goto finish_popd
)

rem ---- 1. Generate the solution -------------------------------------------------
if "%GENERATE%"=="1" (
	if not exist "vendor\premake\bin\premake5.exe" (
		echo [Build] vendor\premake\bin\premake5.exe is missing. Run scripts\Setup.bat once to download it.
		set "EXIT_CODE=2" & goto finish_popd
	)
	echo [Build] Generating the solution ^(premake5 %ACTION%^)
	"vendor\premake\bin\premake5.exe" %ACTION% > "%LOGDIR%\premake.log" 2>&1
	if errorlevel 1 (
		type "%LOGDIR%\premake.log"
		echo [Build] premake failed ^(log: %LOGDIR%\premake.log^)
		set "EXIT_CODE=2" & goto finish_popd
	)
)
if not exist "Hydro.sln" (
	echo [Build] Hydro.sln not found. Run without --no-generate to create it.
	set "EXIT_CODE=2" & goto finish_popd
)

rem ---- 2. Find MSBuild ----------------------------------------------------------
rem vswhere's output goes through a file: a quoted "Program Files (x86)" path inside for /f breaks.
set "MSBUILD="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "!VSWHERE!" (
	"!VSWHERE!" -latest -prerelease -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe > "%LOGDIR%\msbuild-path.txt" 2>nul
	set /p MSBUILD=<"%LOGDIR%\msbuild-path.txt"
)
if not defined MSBUILD (
	for /f "delims=" %%i in ('where msbuild 2^>nul') do if not defined MSBUILD set "MSBUILD=%%i"
)
if not defined MSBUILD (
	echo [Build] MSBuild not found. Install Visual Studio 2022 ^(or its Build Tools^) with the "Desktop development with C++" workload.
	set "EXIT_CODE=2" & goto finish_popd
)
if not exist "!MSBUILD!" (
	echo [Build] MSBuild path from vswhere does not exist: !MSBUILD!
	set "EXIT_CODE=2" & goto finish_popd
)

rem ---- 3. Build -----------------------------------------------------------------
set "MSBUILD_TARGET=%TARGET%"
if defined PROJECT (
	if /i "%TARGET%"=="Build" (set "MSBUILD_TARGET=%PROJECT%") else (set "MSBUILD_TARGET=%PROJECT%:%TARGET%")
)
echo [Build] %CONFIG% ^| target %MSBUILD_TARGET% ^| !MSBUILD!
"!MSBUILD!" Hydro.sln -nologo -m -nodeReuse:false -t:%MSBUILD_TARGET% -p:Configuration=%CONFIG% -v:minimal -clp:Summary;ForceNoAlign -flp1:LogFile=%LOG%;Verbosity=normal -flp2:LogFile=%ERRLOG%;errorsonly -flp3:LogFile=%WARNLOG%;warningsonly
set "BUILD_RESULT=%ERRORLEVEL%"

set "ERRORS=0"
set "WARNINGS=0"
rem MSBuild's own totals ("    12 Warning(s)" / "    0 Error(s)" at the end of the full log).
for /f "tokens=1" %%n in ('findstr /r /c:"^ *[0-9][0-9]* Warning(s)" "%LOG%" 2^>nul') do set "WARNINGS=%%n"
for /f "tokens=1" %%n in ('findstr /r /c:"^ *[0-9][0-9]* Error(s)" "%LOG%" 2^>nul') do set "ERRORS=%%n"

echo.
if "%BUILD_RESULT%"=="0" (
	echo [Build] SUCCEEDED: %CONFIG%, %WARNINGS% warning^(s^)
	echo [Build] Output: bin\%CONFIG%-windows-x86_64\
	set "EXIT_CODE=0"
) else (
	echo [Build] FAILED: %CONFIG%, %ERRORS% error^(s^), %WARNINGS% warning^(s^)
	echo [Build] Errors:
	type "%ERRLOG%"
	set "EXIT_CODE=1"
)
echo [Build] Logs: %LOG%, %ERRLOG%, %WARNLOG%

:finish_popd
popd

:finish
if "%PAUSE_AT_END%"=="1" pause
exit /b %EXIT_CODE%
