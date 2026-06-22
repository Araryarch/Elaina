@echo off
REM Elaina Executor - CMake Build Script
REM Requires: Visual Studio 2022 (v143), .NET 8 SDK, CMake 3.10+

echo ========================================
echo  Elaina Executor - Build Script
echo ========================================
echo.

where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [!] CMake not found in PATH.
    echo [!] Install CMake or run from Visual Studio Developer Command Prompt.
    pause
    exit /b 1
)

where dotnet >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [!] dotnet not found. Install .NET 8 SDK.
    pause
    exit /b 1
)

set CONFIG=Release
if /I "%1"=="debug" set CONFIG=Debug

echo [*] Configuration: %CONFIG%
echo.

REM Generate CMake build
if not exist "build" mkdir build
cd build
echo [*] Configuring CMake...
cmake .. -A x64 -DCMAKE_BUILD_TYPE=%CONFIG%
if %ERRORLEVEL% NEQ 0 (
    echo [-] CMake configuration failed!
    cd ..
    pause
    exit /b 1
)

echo [*] Building Elaina DLL...
cmake --build . --config %CONFIG%
if %ERRORLEVEL% NEQ 0 (
    echo [-] C++ build failed!
    cd ..
    pause
    exit /b 1
)
echo [+] Elaina.dll built successfully.
cd ..

echo [*] Building C# WPF UI...
dotnet build ui\ElainaUI.csproj -c %CONFIG%
if %ERRORLEVEL% NEQ 0 (
    echo [-] UI build failed!
    pause
    exit /b 1
)
echo [+] UI built successfully.

echo.
echo ========================================
echo  Build Complete!
echo ========================================
echo.
echo Output:
echo   build\%CONFIG%\Elaina.dll
echo   ui\bin\%CONFIG%\net8.0-windows\ElainaUI.exe
echo.
pause
