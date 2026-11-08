@echo off
rem Eonlore's Package: Hydro's scripts\Package.bat for this workspace. Hydro is HYDRO_DIR, else ..\Hydro next to this
rem repository, else two folders up (inside the engine repository). Arguments are passed on.
setlocal
set "WORKSPACE=%~dp0.."
if defined HYDRO_DIR if exist "%HYDRO_DIR%\scripts\Package.bat" goto run
set "HYDRO_DIR=%WORKSPACE%\..\Hydro"
if exist "%HYDRO_DIR%\scripts\Package.bat" goto run
set "HYDRO_DIR=%WORKSPACE%\..\.."
if exist "%HYDRO_DIR%\scripts\Package.bat" goto run
echo [Eonlore] Hydro not found: set HYDRO_DIR or clone it next to this repository.
exit /b 2
:run
call "%HYDRO_DIR%\scripts\Package.bat" %* --workspace "%WORKSPACE%"
exit /b %ERRORLEVEL%
