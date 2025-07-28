# Auto Unzip Service - Project Setup Guide

## Required File Structure

Create a new folder for your project and organize the files as follows:

```
AutoUnzipService/
├── AutoUnzipService.cpp     # Main application source (rename from enhanced_main_cpp)
├── resource.h               # Resource header file
├── resource.rc              # Resource definitions
├── CMakeLists.txt          # Build configuration
├── build.bat               # Build script
├── Install-AutoUnzipService.ps1  # PowerShell installer
├── config.ini              # Configuration file
├── README.md               # Documentation
├── LICENSE.txt             # License file (create if needed)
├── tray_icon.ico           # Icon file (create or download)
└── service_install.wxs     # WiX installer fragment
```

## Setup Steps

### 1. Create the Main Source File
- Copy the content from `enhanced_main_cpp` artifact
- Save it as `AutoUnzipService.cpp` (not main.cpp)

### 2. Create Required Files
Create these files in your project directory:

**LICENSE.txt** (simple example):
```
MIT License

Copyright (c) 2025 Auto Unzip Service

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

**tray_icon.ico** - You can either:
- Create a simple icon using an online ICO generator
- Download a free icon from sites like IconFinder or FlatIcon
- Use any 16x16, 32x32, or 48x48 pixel icon file
- Or the build will work with a default Windows icon if this file is missing

### 3. Prerequisites Installation

#### Install Visual Studio 2022
- Download Visual Studio Community 2022 (free)
- During installation, select "Desktop development with C++"
- Make sure to include:
  - Windows 10/11 SDK
  - CMake tools for Visual Studio
  - MSVC v143 compiler toolset

#### Install CMake (if not included with Visual Studio)
```cmd
winget install Kitware.CMake
```

#### Install WiX Toolset (for MSI creation)
```cmd
winget install WiXToolset.WiX
```

### 4. Build Process

#### Option 1: Using the Build Script
```cmd
# Open Command Prompt as Administrator
# Navigate to your project directory
cd path\to\AutoUnzipService
build.bat
```

#### Option 2: Manual Build
```cmd
# Create build directory
mkdir build
cd build

# Configure
cmake .. -G "Visual Studio 17 2022" -A x64

# Build
cmake --build . --config Release

# Create MSI (optional)
cpack -G WIX -C Release
```

#### Option 3: Using Visual Studio IDE
1. Open Visual Studio 2022
2. File > Open > CMake...
3. Select your CMakeLists.txt file
4. Visual Studio will automatically configure the project
5. Build > Build All

### 5. Common Build Issues & Solutions

#### Issue: "CMake not found"
**Solution**: Make sure CMake is installed and in your PATH
```cmd
# Add CMake to PATH or install via Visual Studio Installer
```

#### Issue: "resource.rc compilation failed"
**Solution**: Make sure you have Windows SDK installed
- Open Visual Studio Installer
- Modify your VS installation
- Add "Windows 10/11 SDK (latest version)"

#### Issue: "Cannot find PeaZip"
**Solution**: The app will work without PeaZip for testing, but for full functionality:
- Download PeaZip from https://peazip.github.io/
- Install it in the default location
- Or use the PowerShell script which can auto-download it

#### Issue: "WiX not found" when creating MSI
**Solution**: 
```cmd
# Install WiX Toolset
winget install WiXToolset.WiX
# Or download from: https://wixtoolset.org/releases/
```

#### Issue: "Access denied" when installing service
**Solution**: Run Command Prompt as Administrator
```cmd
# Right-click Command Prompt > "Run as administrator"
AutoUnzipService.exe -install
```

### 6. Testing the Build

#### Test the Executable
```cmd
# Navigate to build\Release\
cd build\Release

# Test help
AutoUnzipService.exe -help

# Test in user mode (should show system tray icon)
AutoUnzipService.exe
```

#### Test Service Installation
```cmd
# Install service (requires admin rights)
AutoUnzipService.exe -install

# Check service status
sc query AutoUnzipService

# Uninstall service
AutoUnzipService.exe -uninstall
```

### 7. PowerShell Script Usage

The PowerShell script provides advanced installation options:

```powershell
# Test configuration
.\Install-AutoUnzipService.ps1 -Action Test

# Install as service with PeaZip auto-download
.\Install-AutoUnzipService.ps1 -Action Install -ServiceMode Service

# Install for current user only
.\Install-AutoUnzipService.ps1 -Action Install -ServiceMode User

# Show status
.\Install-AutoUnzipService.ps1 -Action Status

# Configuration wizard
.\Install-AutoUnzipService.ps1 -Action Configure
```

### 8. Troubleshooting

#### Build Errors
1. Check that all required files are present
2. Verify Visual Studio has C++ development tools
3. Ensure Windows SDK is installed
4. Try cleaning and rebuilding: `cmake --build . --config Release --clean-first`

#### Runtime Errors
1. Check the log file: `AutoUnzipService.log`
2. Verify Downloads folder exists: `%USERPROFILE%\Downloads`
3. Test PeaZip installation manually
4. Run in user mode first before installing as service

#### Service Issues
1. Check Windows Event Viewer for service errors
2. Verify service account has proper permissions
3. Test with PowerShell script for detailed diagnostics

### 9. Development Tips

#### Adding New Features
- The configuration system supports adding new options via `config.ini`
- Resource IDs are defined in `resource.h`
- Dialog layouts are in `resource.rc`
- Main logic is in the `AutoUnzipService` class

#### Debugging
- Build in Debug mode: `cmake --build . --config Debug`
- Enable debug logging in `config.ini`: `DebugMode=true`
- Use Visual Studio debugger for step-through debugging

#### Customization
- Modify supported file extensions in the `supportedExtensions` vector
- Adjust notification messages in the `ShowTrayNotification` function
- Customize PeaZip command line arguments in `ExtractArchive` function

This should resolve the CMake build issues and provide a complete setup guide for building and deploying the Auto Unzip Service!