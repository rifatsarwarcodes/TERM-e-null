@echo off
rem Builds OV_C with MinGW-w64 g++ from a plain Windows command prompt.
rem   build.bat         build ovc.exe
rem   build.bat test    build and run the unit tests

setlocal enabledelayedexpansion
set FLAGS=-std=c++17 -O2 -Wall -Wextra -Isrc
if not exist build mkdir build

set LIB=
for /r src %%f in (*.cpp) do (
    if /i not "%%~nxf"=="main.cpp" set LIB=!LIB! "%%f"
)

if /i "%1"=="test" (
    set TESTSRC=
    for %%f in (tests\*.cpp) do set TESTSRC=!TESTSRC! "%%f"
    g++ %FLAGS% !LIB! !TESTSRC! -o build\ovc_tests.exe || exit /b 1
    build\ovc_tests.exe
    exit /b !errorlevel!
)

g++ %FLAGS% !LIB! src\main.cpp -o build\ovc.exe || exit /b 1
echo Built build\ovc.exe
