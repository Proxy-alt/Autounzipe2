# Auto Unzip Service

An automated archive extraction service for Windows that monitors your Downloads folder and automatically extracts supported archive files using PeaZip.

## Features

- **Automatic Monitoring**: Watches `%USERPROFILE%\Downloads` for new archive files
- **Comprehensive Format Support**: Supports all PeaZip-compatible formats including:
  - Common: ZIP, RAR, 7Z, TAR, GZ, BZ2
  - Disk Images: ISO, IMG, DMG, VHD, VMDK
  - Legacy: CAB, ARJ, LZH, ACE, UUE
  - Split Archives: .001, .002, .part1, .part2
  - Application Packages: WAR, JAR, EAR, APK
  - And many more...
- **Smart Prompting**: Asks for confirmation on non-conventional archive types
- **Password Support**: Prompts for passwords when needed with retry limits
- **2FA Authentication**: Support for two-factor authentication codes
- **System Tray Integration**: Runs quietly in the system tray
- **Windows Service**: Can be installed as a Windows service for automatic startup
- **Minimal Resource Usage**: Efficient file monitoring with low CPU/memory footprint
- **Comprehensive Logging**: Detailed activity logging for troubleshooting

## Prerequisites

1. **Windows 10/11** (x64 recommended)
2. **PeaZip** - Download and install from [https://peazip.github.io/](https://peazip.github.io/)
3. **Visual Studio 2022** (for building from source)
4. **CMake 3.16+** (for building)
5. **WiX Toolset** (for MSI generation)

## Installation

### Method 1: Using the MSI Installer (Recommended)

1. Download the latest release MSI file
2. Run the MSI installer as Administrator
3. Follow the installation wizard
4. The service will be automatically installed and started

### Method 2: Manual Installation

1. Build the project using the provided build script:
   ```cmd
   build.bat
   ```

2. Install as a Windows service:
   ```cmd
   AutoUnzipService.exe -install
   ```

3. Or run in user mode (system tray):
   ```cmd
   AutoUnzipService.exe
   ```

## Usage

### System Tray Mode
- Right-click the tray icon for options:
  - **Show Status**: Display current service status
  - **Pause/Resume**: Temporarily pause monitoring
  - **Exit**: Stop the service

### Automatic Extraction
1. Drop any supported archive into your Downloads folder
2. For common formats (ZIP, RAR, 7Z), extraction starts automatically
3. For uncommon formats, you'll be prompted for confirmation
4. If password-protected, a dialog will appear requesting credentials
5. Files are extracted to a folder with the same name as the archive

### Password-Protected Archives
- Service will first attempt extraction without a password
- If that fails, a password dialog appears
- Enter the password and optional 2FA code
- Maximum of 3 password attempts per archive
- Use "Skip" to ignore password-protected files

## Configuration

### Registry Settings
The service stores configuration in:
```
HKLM\SOFTWARE\AutoUnzipService\
```

### Log Files
Activity logs are stored in:
```
%PROGRAM_FILES%\AutoUnzipService\AutoUnzipService.log
```

### Supported File Extensions
- **Archives**: .7z, .zip, .rar, .tar, .gz, .bz2, .xz, .lzma
- **Disk Images**: .iso, .img, .dmg, .vhd, .vmdk
- **Legacy**: .cab, .arj, .lzh, .ace, .uue, .z
- **Compressed**: .taz, .tbz, .tbz2, .txz, .tlz
- **Packages**: .war, .jar, .ear, .sar, .apk, .ipa
- **Split**: .001-.999, .part1-.part99
- **Others**: .zipx, .par, .par2, .deb, .rpm

## Command Line Options

```cmd
AutoUnzipService.exe [option]

Options:
  -install      Install as Windows service
  -uninstall    Remove Windows service
  -help         Show help information
  (no option)   Run in user mode (system tray)
```

## Building from Source

### Requirements
- Visual Studio 2022 with C++ development tools
- CMake 3.16 or later
- WiX Toolset 3.11+ (for MSI generation)

### Build Steps
1. Clone the repository
2. Ensure PeaZip is installed
3. Run the build script:
   ```cmd
   build.bat
   ```
4. Find the built executable in `build\Release\`
5. Find the MSI installer in `build\`

### Manual Build
```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
cpack -G WIX -C Release
```

## Troubleshooting

### Service Won't Start
1. Ensure you have Administrator privileges
2. Check that PeaZip is properly installed
3. Verify Downloads folder exists and is accessible
4. Check Windows Event Viewer for error details

### Archives Not Being Detected
1. Verify the file extension is supported
2. Check if monitoring is paused (tray icon menu)
3. Ensure files are completely downloaded before processing
4. Check log files for error messages

### Password Dialog Not Appearing
1. Ensure the service is running in user mode, not as a system service
2. Check if the archive actually requires a password
3. Try running as Administrator

### PeaZip Not Found
1. Install PeaZip from the official website
2. Ensure it's installed in the default location
3. Check registry entries for PeaZip installation path

## Security Considerations

- The service runs with the privileges of the current user
- Passwords are not stored or logged
- 2FA codes are handled securely and not retained
- All file operations respect Windows file permissions
- Network drives and cloud storage may have delayed file detection

## Performance

- **CPU Usage**: <1% during idle monitoring
- **Memory Usage**: ~5-15MB RAM
- **Disk I/O**: Minimal, only during extraction
- **Network**: No network activity required

## Uninstallation

### From MSI
1. Use "Add or Remove Programs" in Windows Settings
2. Find "Auto Unzip Service" and uninstall

### Manual Removal
```cmd
AutoUnzipService.exe -uninstall
```

## Support

For issues, feature requests, or questions:
1. Check the log files first
2. Verify PeaZip installation
3. Try running in user mode for debugging
4. Check Windows Event Viewer for system-level errors

## License

This software is provided as-is for personal and commercial use. No warranty is provided.

## Version History

### v1.0.0
- Initial release
- Basic archive monitoring and extraction
- Password support with retry limits
- 2FA authentication support
- System tray integration
- Windows service installation
- MSI installer package
- Comprehensive logging