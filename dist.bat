@echo off
REM Elaina - Hybrid Dist Build
REM Builds C++ DLL + C# UI, packages into dist/
REM Requires: Visual Studio 2022, .NET 8 SDK, CMake 3.10+

setlocal enabledelayedexpansion

echo ========================================
echo  Elaina - Dist Build
echo ========================================
echo.

REM Add common CMake paths
set "PATH=C:\Program Files\CMake\bin;%PATH%"

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

REM ===========================================
REM Step 1: Build C++ DLL
REM ===========================================
echo [*] Step 1/3: Building Elaina.dll...
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
echo [*] Compiling...
cmake --build . --config %CONFIG%
if %ERRORLEVEL% NEQ 0 (
    echo [-] C++ build failed!
    popd
    pause
    exit /b 1
)
popd
echo [+] Elaina.dll built successfully.

REM ===========================================
REM Step 2: Build C# UI (single-file publish)
REM ===========================================
echo [*] Step 2/3: Building C# UI (single-file)...
dotnet publish ui\ElainaUI.csproj -c %CONFIG% -r win-x64 --self-contained false -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -o dist\tmp
if %ERRORLEVEL% NEQ 0 (
    echo [-] UI build failed!
    pause
    exit /b 1
)
echo [+] UI built successfully.

REM ===========================================
REM Step 3: Package into dist/
REM ===========================================
echo [*] Step 3/3: Packaging into dist/...

if not exist "dist" mkdir dist

REM Copy DLL
copy /Y "build\%CONFIG%\Elaina.dll" "dist\Elaina.dll" >nul
echo [+] Copied: Elaina.dll (%CONFIG%)

REM Copy EXE and config
copy /Y "dist\tmp\Elaina-Executor.exe" "dist\Elaina-Executor.exe" >nul
if exist "dist\tmp\Elaina-Executor.dll" copy /Y "dist\tmp\Elaina-Executor.dll" "dist\Elaina-Executor.dll" >nul 2>&1
if exist "dist\tmp\Elaina-Executor.runtimeconfig.json" copy /Y "dist\tmp\Elaina-Executor.runtimeconfig.json" "dist\Elaina-Executor.runtimeconfig.json" >nul 2>&1
if exist "dist\tmp\*.pdb" copy /Y "dist\tmp\*.pdb" "dist\" >nul 2>&1

REM Also copy example scripts
if exist "scripts\unctest.lua" copy /Y "scripts\unctest.lua" "dist\unctest.lua" >nul 2>&1
if exist "scripts\example.lua" copy /Y "scripts\example.lua" "dist\example.lua" >nul 2>&1

REM Clean temp
rmdir /S /Q "dist\tmp" >nul 2>&1

echo [+] Packaging complete.
echo.

REM ===========================================
REM Summary
REM ===========================================
echo ========================================
echo  Build Complete!
echo ========================================
echo.
echo Output in dist/:
dir /B "dist\"

echo.
echo dist\Elaina.dll    - C++ executor engine (embedded in EXE)
echo dist\Elaina-Executor.exe - Standalone executable (single-file, .NET 8 required)
echo.
echo To run: just launch Elaina-Executor.exe
echo.

endlocal
pause
