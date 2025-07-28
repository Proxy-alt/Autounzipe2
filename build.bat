@echo off
echo Building Auto Unzip Service...

REM Check if required files exist
if not exist "AutoUnzipService.cpp" (
    echo Error: AutoUnzipService.cpp not found!
    echo Please make sure the main C++ file is named AutoUnzipService.cpp
    pause
    exit /b 1
)

if not exist "resource.h" (
    echo Error: resource.h not found!
    pause
    exit /b 1
)

if not exist "resource.rc" (
    echo Error: resource.rc not found!
    pause
    exit /b 1
)

REM Create build directory
if not exist build mkdir build
cd build

echo Configuring with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

echo Building the project...
cmake --build . --config Release
if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo Creating MSI package...
cpack -G WIX -C Release
if %ERRORLEVEL% neq 0 (
    echo Warning: MSI creation failed. You can still use the executable.
)

echo.
echo Build complete! 
echo Executable: build\Release\AutoUnzipService.exe
echo MSI Installer: build\Auto Unzip Service-1.0.0-win64.msi
echo.
echo To install the service, run the MSI installer or:
echo AutoUnzipService.exe -install
echo.
echo To uninstall:
echo AutoUnzipService.exe -uninstall

pause