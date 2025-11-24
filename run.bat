@echo off
setlocal enabledelayedexpansion

rem Build-and-run for Windows (cmd.exe)
rem Usage: run.bat [args]

cd /d "%~dp0"

where g++ >nul 2>&1
if %errorlevel%==0 goto use_gpp

echo g++ not found on PATH.
echo If you have WSL or Git Bash, run the existing `run.sh` there, or install MinGW/MinGW-w64.
echo Example (WSL):
echo   wsl bash -lc "cd /mnt/c/Users/%%USERNAME%%/OneDrive/\"Documents/GitHub/order-book\" && ./run.sh %*"
exit /b 1

:use_gpp
if not exist build mkdir build

rem Collect source files into FILES variable (start with main.cpp)
set "FILES=main.cpp"
for /r "src" %%f in (*.cpp) do (
  set "FILES=!FILES! "%%f""
)

echo Compiling: %FILES%
g++ -std=c++17 -O2 -Wall -Iinclude %FILES% -o build\main.exe
if errorlevel 1 (
  echo Build failed.
  exit /b 1
)

echo Running: build\main.exe %*
build\main.exe %*

endlocal
