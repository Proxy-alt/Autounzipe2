#Requires -RunAsAdministrator

<#
.SYNOPSIS
    Advanced installation script for Auto Unzip Service
.DESCRIPTION
    This script provides comprehensive installation, configuration, and management
    capabilities for the Auto Unzip Service, including PeaZip detection and setup.
.PARAMETER Action
    The action to perform: Install, Uninstall, Status, Configure, or Test
.PARAMETER ServiceMode
    Whether to install as a Windows service or user application
.PARAMETER SkipPeaZipCheck
    Skip the PeaZip installation verification
.EXAMPLE
    .\Install-AutoUnzipService.ps1 -Action Install -ServiceMode Service
.EXAMPLE
    .\Install-AutoUnzipService.ps1 -Action Test
#>

param(
    [Parameter(Mandatory=$true)]
    [ValidateSet("Install", "Uninstall", "Status", "Configure", "Test")]
    [string]$Action,
    
    [Parameter(Mandatory=$false)]
    [ValidateSet("Service", "User")]
    [string]$ServiceMode = "User",
    
    [Parameter(Mandatory=$false)]
    [switch]$SkipPeaZipCheck
)

# Constants
$ServiceName = "AutoUnzipService"
$ServiceDisplayName = "Auto Unzip Service"
$ServiceDescription = "Automatically extracts archives in the Downloads folder using PeaZip"
$RegistryPath = "HKLM:\SOFTWARE\AutoUnzipService"
$LogPath = "$env:ProgramData\AutoUnzipService\Logs"

# Helper Functions
function Write-ColorOutput {
    param(
        [string]$Message,
        [string]$Color = "White"
    )
    Write-Host $Message -ForegroundColor $Color
}

function Test-AdminRights {
    $currentPrincipal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    return $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Find-PeaZip {
    $peazipPaths = @(
        "${env:ProgramFiles}\PeaZip\peazip.exe",
        "${env:ProgramFiles(x86)}\PeaZip\peazip.exe",
        "C:\PeaZip\peazip.exe"
    )
    
    # Check registry
    $regPaths = @(
        "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\PeaZip",
        "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\PeaZip"
    )
    
    foreach ($regPath in $regPaths) {
        try {
            $installLocation = Get-ItemProperty -Path $regPath -Name "InstallLocation" -ErrorAction SilentlyContinue
            if ($installLocation) {
                $peazipPath = Join-Path $installLocation.InstallLocation "peazip.exe"
                if (Test-Path $peazipPath) {
                    return $peazipPath
                }
            }
        } catch {
            # Registry key doesn't exist, continue
        }
    }
    
    # Check default paths
    foreach ($path in $peazipPaths) {
        if (Test-Path $path) {
            return $path
        }
    }
    
    return $null
}

function Install-PeaZipIfMissing {
    $peazipPath = Find-PeaZip
    if (-not $peazipPath) {
        Write-ColorOutput "PeaZip not found. Attempting to download and install..." "Yellow"
        
        # Download PeaZip
        $downloadUrl = "https://github.com/peazip/PeaZip/releases/latest/download/peazip-8.9.0.WIN64.exe"
        $installerPath = "$env:TEMP\peazip-installer.exe"
        
        try {
            Write-ColorOutput "Downloading PeaZip installer..." "Cyan"
            Invoke-WebRequest -Uri $downloadUrl -OutFile $installerPath -UseBasicParsing
            
            Write-ColorOutput "Installing PeaZip..." "Cyan"
            Start-Process -FilePath $installerPath -ArgumentList "/S" -Wait
            
            # Verify installation
            Start-Sleep -Seconds 5
            $peazipPath = Find-PeaZip
            if ($peazipPath) {
                Write-ColorOutput "PeaZip installed successfully at: $peazipPath" "Green"
            } else {
                Write-ColorOutput "PeaZip installation may have failed. Please install manually." "Red"
                return $false
            }
        } catch {
            Write-ColorOutput "Failed to download/install PeaZip: $($_.Exception.Message)" "Red"
            Write-ColorOutput "Please download and install PeaZip manually from https://peazip.github.io/" "Yellow"
            return $false
        } finally {
            if (Test-Path $installerPath) {
                Remove-Item $installerPath -Force
            }
        }
    } else {
        Write-ColorOutput "PeaZip found at: $peazipPath" "Green"
    }
    
    return $true
}

function Test-ServiceConfiguration {
    Write-ColorOutput "Testing Auto Unzip Service Configuration..." "Cyan"
    
    $issues = @()
    
    # Check if executable exists
    $exePath = Get-ServiceExecutablePath
    if (-not (Test-Path $exePath)) {
        $issues += "Service executable not found at: $exePath"
    } else {
        Write-ColorOutput "✓ Service executable found" "Green"
    }
    
    # Check PeaZip
    if (-not $SkipPeaZipCheck) {
        $peazipPath = Find-PeaZip
        if (-not $peazipPath) {
            $issues += "PeaZip not found - required for archive extraction"
        } else {
            Write-ColorOutput "✓ PeaZip found at: $peazipPath" "Green"
        }
    }
    
    # Check Downloads folder
    $downloadsPath = "$env:USERPROFILE\Downloads"
    if (-not (Test-Path $downloadsPath)) {
        $issues += "Downloads folder not found: $downloadsPath"
    } else {
        Write-ColorOutput "✓ Downloads folder accessible" "Green"
    }
    
    # Check service status if installed
    $service = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
    if ($service) {
        Write-ColorOutput "✓ Service installed - Status: $($service.Status)" "Green"
    } else {
        Write-ColorOutput "ℹ Service not installed" "Yellow"
    }
    
    # Check registry configuration
    if (Test-Path $RegistryPath) {
        Write-ColorOutput "✓ Registry configuration found" "Green"
    } else {
        Write-ColorOutput "ℹ Registry configuration not found" "Yellow"
    }
    
    # Check log directory
    if (Test-Path $LogPath) {
        Write-ColorOutput "✓ Log directory exists" "Green"
    } else {
        Write-ColorOutput "ℹ Log directory will be created when needed" "Yellow"
    }
    
    if ($issues.Count -eq 0) {
        Write-ColorOutput "✓ All configuration tests passed!" "Green"
        return $true
    } else {
        Write-ColorOutput "⚠ Configuration issues found:" "Red"
        foreach ($issue in $issues) {
            Write-ColorOutput "  • $issue" "Red"
        }
        return $false
    }
}

function Get-ServiceExecutablePath {
    $currentPath = Split-Path -Parent $MyInvocation.ScriptName
    return Join-Path $currentPath "AutoUnzipService.exe"
}

function Install-Service {
    Write-ColorOutput "Installing Auto Unzip Service..." "Cyan"
    
    # Verify prerequisites
    if (-not $SkipPeaZipCheck) {
        if (-not (Install-PeaZipIfMissing)) {
            return $false
        }
    }
    
    $exePath = Get-ServiceExecutablePath
    if (-not (Test-Path $exePath)) {
        Write-ColorOutput "Error: AutoUnzipService.exe not found at: $exePath" "Red"
        return $false
    }
    
    try {
        if ($ServiceMode -eq "Service") {
            # Install as Windows service
            Write-ColorOutput "Installing as Windows service..." "Cyan"
            & $exePath -install
            
            # Verify service installation
            $service = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
            if ($service) {
                Write-ColorOutput "✓ Windows service installed successfully" "Green"
                
                # Start the service
                Start-Service -Name $ServiceName
                Write-ColorOutput "✓ Service started" "Green"
            } else {
                Write-ColorOutput "✗ Service installation failed" "Red"
                return $false
            }
        } else {
            # Install for user mode
            Write-ColorOutput "Setting up for user mode..." "Cyan"
            
            # Create startup shortcut
            $startupPath = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup"
            $shortcutPath = Join-Path $startupPath "Auto Unzip Service.lnk"
            
            $wshell = New-Object -comObject WScript.Shell
            $shortcut = $wshell.CreateShortcut($shortcutPath)
            $shortcut.TargetPath = $exePath
            $shortcut.Description = $ServiceDescription
            $shortcut.Save()
            
            Write-ColorOutput "✓ Startup shortcut created" "Green"
        }
        
        # Create registry configuration
        if (-not (Test-Path $RegistryPath)) {
            New-Item -Path $RegistryPath -Force | Out-Null
        }
        
        Set-ItemProperty -Path $RegistryPath -Name "InstallPath" -Value (Split-Path $exePath)
        Set-ItemProperty -Path $RegistryPath -Name "Version" -Value "1.0.0"
        Set-ItemProperty -Path $RegistryPath -Name "ServiceMode" -Value $ServiceMode
        Set-ItemProperty -Path $RegistryPath -Name "InstallDate" -Value (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
        
        # Create log directory
        if (-not (Test-Path $LogPath)) {
            New-Item -Path $LogPath -ItemType Directory -Force | Out-Null
        }
        
        Write-ColorOutput "✓ Auto Unzip Service installed successfully!" "Green"
        
        if ($ServiceMode -eq "User") {
            Write-ColorOutput "The service will start automatically when you log in." "Cyan"
            Write-ColorOutput "You can also run it manually: $exePath" "Cyan"
        }
        
        return $true
        
    } catch {
        Write-ColorOutput "Error during installation: $($_.Exception.Message)" "Red"
        return $false
    }
}

function Uninstall-Service {
    Write-ColorOutput "Uninstalling Auto Unzip Service..." "Cyan"
    
    try {
        # Stop and remove Windows service if it exists
        $service = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
        if ($service) {
            Write-ColorOutput "Stopping Windows service..." "Cyan"
            Stop-Service -Name $ServiceName -Force -ErrorAction SilentlyContinue
            
            $exePath = Get-ServiceExecutablePath
            if (Test-Path $exePath) {
                & $exePath -uninstall
            }
            
            Write-ColorOutput "✓ Windows service removed" "Green"
        }
        
        # Remove startup shortcut
        $startupPath = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup"
        $shortcutPath = Join-Path $startupPath "Auto Unzip Service.lnk"
        if (Test-Path $shortcutPath) {
            Remove-Item $shortcutPath -Force
            Write-ColorOutput "✓ Startup shortcut removed" "Green"
        }
        
        # Remove registry configuration
        if (Test-Path $RegistryPath) {
            Remove-Item -Path $RegistryPath -Recurse -Force
            Write-ColorOutput "✓ Registry configuration removed" "Green"
        }
        
        # Ask about log files
        $removeLogsResponse = Read-Host "Remove log files? (y/n)"
        if ($removeLogsResponse -eq 'y' -or $removeLogsResponse -eq 'Y') {
            if (Test-Path $LogPath) {
                Remove-Item -Path $LogPath -Recurse -Force
                Write-ColorOutput "✓ Log files removed" "Green"
            }
        }
        
        Write-ColorOutput "✓ Auto Unzip Service uninstalled successfully!" "Green"
        return $true
        
    } catch {
        Write-ColorOutput "Error during uninstallation: $($_.Exception.Message)" "Red"
        return $false
    }
}

function Show-ServiceStatus {
    Write-ColorOutput "Auto Unzip Service Status" "Cyan"
    Write-ColorOutput "=========================" "Cyan"
    
    # Service status
    $service = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
    if ($service) {
        Write-ColorOutput "Windows Service: $($service.Status)" "Green"
    } else {
        Write-ColorOutput "Windows Service: Not Installed" "Yellow"
    }
    
    # Registry info
    if (Test-Path $RegistryPath) {
        $config = Get-ItemProperty -Path $RegistryPath
        Write-ColorOutput "Install Path: $($config.InstallPath)" "White"
        Write-ColorOutput "Version: $($config.Version)" "White"
        Write-ColorOutput "Service Mode: $($config.ServiceMode)" "White"
        Write-ColorOutput "Install Date: $($config.InstallDate)" "White"
    } else {
        Write-ColorOutput "Registry Configuration: Not Found" "Yellow"
    }
    
    # PeaZip status
    $peazipPath = Find-PeaZip
    if ($peazipPath) {
        Write-ColorOutput "PeaZip: Found at $peazipPath" "Green"
    } else {
        Write-ColorOutput "PeaZip: Not Found" "Red"
    }
    
    # Downloads folder
    $downloadsPath = "$env:USERPROFILE\Downloads"
    if (Test-Path $downloadsPath) {
        $fileCount = (Get-ChildItem $downloadsPath -File | Measure-Object).Count
        Write-ColorOutput "Downloads Folder: $downloadsPath ($fileCount files)" "Green"
    } else {
        Write-ColorOutput "Downloads Folder: Not Found" "Red"
    }
    
    # Log files
    if (Test-Path $LogPath) {
        $logFiles = Get-ChildItem $LogPath -Filter "*.log" | Sort-Object LastWriteTime -Descending
        if ($logFiles) {
            $latestLog = $logFiles[0]
            Write-ColorOutput "Latest Log: $($latestLog.FullName) ($(Get-Date $latestLog.LastWriteTime -Format 'yyyy-MM-dd HH:mm:ss'))" "Green"
        } else {
            Write-ColorOutput "Log Files: Directory exists but no logs found" "Yellow"
        }
    } else {
        Write-ColorOutput "Log Files: Directory not found" "Yellow"
    }
}

function Start-ConfigurationWizard {
    Write-ColorOutput "Auto Unzip Service Configuration Wizard" "Cyan"
    Write-ColorOutput "=======================================" "Cyan"
    
    # Installation mode
    $modeChoice = Read-Host "Install as (S)ervice or (U)ser application? [S/U]"
    $selectedMode = if ($modeChoice -eq 'U' -or $modeChoice -eq 'u') { "User" } else { "Service" }
    
    Write-ColorOutput "Selected mode: $selectedMode" "Green"
    
    # PeaZip check
    if (-not $SkipPeaZipCheck) {
        $peazipPath = Find-PeaZip
        if (-not $peazipPath) {
            $installPeaZip = Read-Host "PeaZip not found. Download and install automatically? [Y/n]"
            if ($installPeaZip -ne 'n' -and $installPeaZip -ne 'N') {
                Install-PeaZipIfMissing
            }
        }
    }
    
    # Proceed with installation
    $proceed = Read-Host "Proceed with installation? [Y/n]"
    if ($proceed -ne 'n' -and $proceed -ne 'N') {
        $global:ServiceMode = $selectedMode
        return Install-Service
    }
    
    return $false
}

# Main execution
function Main {
    Write-ColorOutput "Auto Unzip Service Installation Script" "Cyan"
    Write-ColorOutput "======================================" "Cyan"
    
    if (-not (Test-AdminRights) -and ($Action -eq "Install" -or $Action -eq "Uninstall")) {
        Write-ColorOutput "This script requires Administrator privileges for service installation/removal." "Red"
        Write-ColorOutput "Please run PowerShell as Administrator and try again." "Yellow"
        exit 1
    }
    
    switch ($Action) {
        "Install" {
            if ($ServiceMode -eq "Auto") {
                $result = Start-ConfigurationWizard
            } else {
                $result = Install-Service
            }
            exit $(if ($result) { 0 } else { 1 })
        }
        "Uninstall" {
            $result = Uninstall-Service
            exit $(if ($result) { 0 } else { 1 })
        }
        "Status" {
            Show-ServiceStatus
            exit 0
        }
        "Configure" {
            $result = Start-ConfigurationWizard
            exit $(if ($result) { 0 } else { 1 })
        }
        "Test" {
            $result = Test-ServiceConfiguration
            exit $(if ($result) { 0 } else { 1 })
        }
        default {
            Write-ColorOutput "Unknown action: $Action" "Red"
            exit 1
        }
    }
}

# Execute main function
Main