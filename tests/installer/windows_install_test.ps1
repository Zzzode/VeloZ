# VeloZ Windows Installation Test Script
# Tests installation, verification, and uninstallation on Windows

param(
    [Parameter(Mandatory=$false)]
    [string]$InstallerPath = ".\VeloZ-Setup.exe",

    [Parameter(Mandatory=$false)]
    [string]$ExpectedVersion = "1.0.0",

    [Parameter(Mandatory=$false)]
    [switch]$SkipUninstall,

    [Parameter(Mandatory=$false)]
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"
$script:TestsPassed = 0
$script:TestsFailed = 0

# Test result logging
function Write-TestResult {
    param(
        [string]$TestName,
        [bool]$Passed,
        [string]$Message = ""
    )

    if ($Passed) {
        Write-Host "[PASS] $TestName" -ForegroundColor Green
        $script:TestsPassed++
    } else {
        Write-Host "[FAIL] $TestName" -ForegroundColor Red
        if ($Message) {
            Write-Host "       $Message" -ForegroundColor Yellow
        }
        $script:TestsFailed++
    }
}

function Write-Section {
    param([string]$Title)
    Write-Host "`n=== $Title ===" -ForegroundColor Cyan
}

# Pre-installation checks
function Test-Prerequisites {
    Write-Section "Prerequisites"

    # Check installer exists
    $installerExists = Test-Path $InstallerPath
    Write-TestResult "Installer file exists" $installerExists "Path: $InstallerPath"

    if (-not $installerExists) {
        throw "Installer not found at: $InstallerPath"
    }

    # Check installer is signed
    $signature = Get-AuthenticodeSignature -FilePath $InstallerPath
    $isSigned = $signature.Status -eq "Valid"
    Write-TestResult "Installer is code-signed" $isSigned "Status: $($signature.Status)"

    # Check disk space (need at least 500MB)
    $drive = (Get-Item $InstallerPath).PSDrive.Name
    $freeSpace = (Get-PSDrive $drive).Free
    $hasSpace = $freeSpace -gt 500MB
    Write-TestResult "Sufficient disk space" $hasSpace "Free: $([math]::Round($freeSpace/1GB, 2)) GB"

    # Check for existing installation
    $existingInstall = Test-Path "${env:ProgramFiles}\VeloZ"
    if ($existingInstall) {
        Write-Host "[INFO] Existing installation detected - will be upgraded" -ForegroundColor Yellow
    }
}

# Run the installer
function Install-VeloZ {
    Write-Section "Installation"

    $startTime = Get-Date

    # Run installer silently
    Write-Host "Running installer..." -ForegroundColor Gray
    $process = Start-Process -FilePath $InstallerPath -ArgumentList "/S" -Wait -PassThru

    $duration = (Get-Date) - $startTime
    $exitedCleanly = $process.ExitCode -eq 0
    Write-TestResult "Installer completed successfully" $exitedCleanly "Exit code: $($process.ExitCode)"

    $fastInstall = $duration.TotalMinutes -lt 5
    Write-TestResult "Installation completed in < 5 minutes" $fastInstall "Duration: $([math]::Round($duration.TotalSeconds, 1)) seconds"

    if (-not $exitedCleanly) {
        throw "Installation failed with exit code: $($process.ExitCode)"
    }
}

# Verify installation
function Test-Installation {
    Write-Section "Installation Verification"

    $installDir = "${env:ProgramFiles}\VeloZ"

    # Check installation directory
    $dirExists = Test-Path $installDir
    Write-TestResult "Installation directory exists" $dirExists "Path: $installDir"

    if (-not $dirExists) {
        throw "Installation directory not found"
    }

    # Check main executable
    $exePath = Join-Path $installDir "veloz.exe"
    $exeExists = Test-Path $exePath
    Write-TestResult "Main executable exists" $exeExists "Path: $exePath"

    # Check resources
    $resourcesPath = Join-Path $installDir "resources"
    $resourcesExist = Test-Path $resourcesPath
    Write-TestResult "Resources directory exists" $resourcesExist

    # Check engine
    $enginePath = Join-Path $resourcesPath "engine\veloz_engine.exe"
    $engineExists = Test-Path $enginePath
    Write-TestResult "Trading engine exists" $engineExists

    # Check gateway
    $gatewayPath = Join-Path $resourcesPath "gateway"
    $gatewayExists = Test-Path $gatewayPath
    Write-TestResult "Gateway exists" $gatewayExists

    # Check UI
    $uiPath = Join-Path $resourcesPath "ui\index.html"
    $uiExists = Test-Path $uiPath
    Write-TestResult "UI assets exist" $uiExists

    # Check desktop shortcut
    $shortcutPath = "$env:USERPROFILE\Desktop\VeloZ.lnk"
    $shortcutExists = Test-Path $shortcutPath
    Write-TestResult "Desktop shortcut created" $shortcutExists

    # Check Start menu entry
    $startMenuPath = "$env:ProgramData\Microsoft\Windows\Start Menu\Programs\VeloZ"
    $startMenuExists = Test-Path $startMenuPath
    Write-TestResult "Start menu entry created" $startMenuExists

    # Check uninstaller registry
    $uninstallKey = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\VeloZ"
    $uninstallExists = Test-Path $uninstallKey
    Write-TestResult "Uninstaller registered" $uninstallExists

    # Check version
    if ($exeExists) {
        try {
            $versionInfo = (Get-Item $exePath).VersionInfo
            $version = $versionInfo.ProductVersion
            $versionMatch = $version -like "*$ExpectedVersion*"
            Write-TestResult "Version matches expected" $versionMatch "Expected: $ExpectedVersion, Got: $version"
        } catch {
            Write-TestResult "Version check" $false "Could not read version info"
        }
    }
}

# Test application launch
function Test-Launch {
    Write-Section "Application Launch"

    $exePath = "${env:ProgramFiles}\VeloZ\veloz.exe"

    if (-not (Test-Path $exePath)) {
        Write-TestResult "Application launch" $false "Executable not found"
        return
    }

    # Start application
    Write-Host "Starting VeloZ..." -ForegroundColor Gray
    $process = Start-Process -FilePath $exePath -PassThru

    # Wait for window to appear
    Start-Sleep -Seconds 5

    # Check if process is running
    $isRunning = -not $process.HasExited
    Write-TestResult "Application started" $isRunning

    # Check for main window
    if ($isRunning) {
        $hasWindow = $process.MainWindowHandle -ne 0
        Write-TestResult "Main window created" $hasWindow
    }

    # Clean up
    if ($isRunning) {
        $process.Kill()
        $process.WaitForExit(5000)
    }
}

# Test engine functionality
function Test-Engine {
    Write-Section "Engine Functionality"

    $enginePath = "${env:ProgramFiles}\VeloZ\resources\engine\veloz_engine.exe"

    if (-not (Test-Path $enginePath)) {
        Write-TestResult "Engine test" $false "Engine not found"
        return
    }

    # Run engine smoke test
    Write-Host "Running engine smoke test..." -ForegroundColor Gray
    try {
        $process = Start-Process -FilePath $enginePath -ArgumentList "--help" -Wait -PassThru -NoNewWindow -RedirectStandardOutput "engine_output.txt"
        $exitedCleanly = $process.ExitCode -eq 0
        Write-TestResult "Engine smoke test" $exitedCleanly "Exit code: $($process.ExitCode)"

        if (Test-Path "engine_output.txt") {
            Remove-Item "engine_output.txt"
        }
    } catch {
        Write-TestResult "Engine smoke test" $false $_.Exception.Message
    }
}

# Test uninstallation
function Test-Uninstall {
    Write-Section "Uninstallation"

    $uninstallKey = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\VeloZ"

    if (-not (Test-Path $uninstallKey)) {
        Write-TestResult "Uninstall test" $false "Uninstaller not registered"
        return
    }

    # Get uninstall command
    $uninstallString = (Get-ItemProperty $uninstallKey).UninstallString

    if (-not $uninstallString) {
        Write-TestResult "Uninstall command found" $false
        return
    }

    Write-TestResult "Uninstall command found" $true

    # Run uninstaller
    Write-Host "Running uninstaller..." -ForegroundColor Gray
    $process = Start-Process -FilePath "cmd.exe" -ArgumentList "/c", $uninstallString, "/S" -Wait -PassThru
    $exitedCleanly = $process.ExitCode -eq 0
    Write-TestResult "Uninstaller completed" $exitedCleanly "Exit code: $($process.ExitCode)"

    # Verify removal
    Start-Sleep -Seconds 2

    $installDir = "${env:ProgramFiles}\VeloZ"
    $dirRemoved = -not (Test-Path $installDir)
    Write-TestResult "Installation directory removed" $dirRemoved

    $shortcutPath = "$env:USERPROFILE\Desktop\VeloZ.lnk"
    $shortcutRemoved = -not (Test-Path $shortcutPath)
    Write-TestResult "Desktop shortcut removed" $shortcutRemoved

    $startMenuPath = "$env:ProgramData\Microsoft\Windows\Start Menu\Programs\VeloZ"
    $startMenuRemoved = -not (Test-Path $startMenuPath)
    Write-TestResult "Start menu entry removed" $startMenuRemoved

    $uninstallRemoved = -not (Test-Path $uninstallKey)
    Write-TestResult "Registry entry removed" $uninstallRemoved
}

# Main execution
function Main {
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "   VeloZ Windows Installation Test" -ForegroundColor Cyan
    Write-Host "========================================`n" -ForegroundColor Cyan

    Write-Host "Installer: $InstallerPath"
    Write-Host "Expected Version: $ExpectedVersion"
    Write-Host ""

    try {
        Test-Prerequisites
        Install-VeloZ
        Test-Installation
        Test-Launch
        Test-Engine

        if (-not $SkipUninstall) {
            Test-Uninstall
        } else {
            Write-Host "`n[INFO] Skipping uninstall test" -ForegroundColor Yellow
        }
    } catch {
        Write-Host "`n[ERROR] Test aborted: $_" -ForegroundColor Red
        $script:TestsFailed++
    }

    # Summary
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "   Test Summary" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "Passed: $script:TestsPassed" -ForegroundColor Green
    Write-Host "Failed: $script:TestsFailed" -ForegroundColor $(if ($script:TestsFailed -gt 0) { "Red" } else { "Green" })

    if ($script:TestsFailed -gt 0) {
        Write-Host "`nSome tests FAILED" -ForegroundColor Red
        exit 1
    } else {
        Write-Host "`nAll tests PASSED" -ForegroundColor Green
        exit 0
    }
}

Main
