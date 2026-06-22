@echo off
REM Syntax Executor - Build Script
REM Requires: Visual Studio 2022 (v143), .NET 8 SDK, CMake 3.10+
REM
REM Commands:
REM   build.bat          - Quick C++ DLL build only
REM   build.bat debug    - Debug build
REM   dist.bat           - Full hybrid build (DLL + UI) into dist/

echo ========================================
echo  Syntax Executor - Build Script
echo ========================================
echo.
echo Commands:
echo   build.bat          - Build C++ DLL only (quick)
echo   build.bat debug    - Debug build
echo   dist.bat           - Full hybrid build (DLL + UI) into dist/
echo.

where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [!] CMake not found in PATH.
    echo [!] Install CMake or run from Visual Studio Developer Command Prompt.
    pause
    exit /b 1
)

set CONFIG=Release
if /I "%1"=="debug" set CONFIG=Debug

echo [*] Configuration: %CONFIG%
echo.

REM Generate CMake build
if not exist "build" mkdir build
pushd build
echo [*] Configuring CMake...
cmake .. -A x64 -DCMAKE_BUILD_TYPE=%CONFIG%
if %ERRORLEVEL% NEQ 0 (
    echo [-] CMake configuration failed!
    popd
    pause
    exit /b 1
)

echo [*] Building SyntaxAPI.dll...
cmake --build . --config %CONFIG%
if %ERRORLEVEL% NEQ 0 (
    echo [-] C++ build failed!
    popd
    pause
    exit /b 1
)
popd
echo [+] SyntaxAPI.dll built successfully.
echo.
echo Output: build\%CONFIG%\SyntaxAPI.dll
echo.
echo For full hybrid build (DLL + UI packaged), run: dist.bat
echo.
pause
