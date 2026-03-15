#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Windows Server Bypass Installer
    Installs the Win11VersionLie shim to make apps think they're running on Windows 11 Workstation
    instead of Windows Server 2025.

.DESCRIPTION
    This script:
    1. Lets you select preset exe targets (Call of Duty) or add custom ones
    2. Generates the SDB shim database for your selected executables
    3. Backs up the original AcRes.dll (if not already backed up)
    4. Copies the custom shim DLL into System32
    5. Uninstalls any previous version of the shim
    6. Installs the new SDB
#>

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# --- File paths ---
$AcResDll       = Join-Path $ScriptDir "AcRes.dll"
$SprintDll      = Join-Path $ScriptDir "SprintDLL.exe"
$SprintScript   = Join-Path $ScriptDir "Win11VersionLie.sprint"
$SdbFile        = Join-Path $ScriptDir "Win11VersionLie.sdb"
$Sys32AcRes     = "$env:SystemRoot\System32\AcRes.dll"
$Sys32AcResOrig = "$env:SystemRoot\System32\AcRes.orig"
$SdbName        = "Win11VersionLie"

# --- Presets ---
$Presets = @{
    "Call of Duty" = @("cod.exe", "cod24-cod.exe", "cod25-cod.exe")
    "Oculus/Meta"  = @(
        "OculusSetup.exe", "OculusClient.exe", "OVRServer_x64.exe",
        "OVRRedir.exe", "OVRServiceLauncher.exe", "OculusDash.exe",
        "OculusDebugTool.exe", "OVRTray.exe", "OVRUpdate.exe",
        "MetaQuestLink.exe", "OculusLinkCable.exe", "AirLinkLauncher.exe",
        "OculusDiagnostics.exe", "OVRPlatformSampleApp.exe", "OculusWorldDemo.exe",
        "OculusRoomTiny.exe", "OVRPlugin.exe", "OculusInstaller.exe",
        "OVRMonitorConsole.exe", "OculusPlatformUtil.exe", "OVRLauncher.exe",
        "OculusXRPlugin.exe", "OVRServiceHost.exe", "OculusKeepalive.exe",
        "OVRErrorHandler.exe", "OculusDashHelper.exe", "OVRTracking.exe",
        "OculusLogger.exe", "OVRHeadsetProximity.exe", "OculusBrowser.exe"
    )
}

# --- Validate required files ---
function Test-RequiredFiles {
    $missing = @()
    if (-not (Test-Path $AcResDll))  { $missing += "AcRes.dll" }
    if (-not (Test-Path $SprintDll)) { $missing += "SprintDLL.exe" }
    if ($missing.Count -gt 0) {
        Write-Host "`n[ERROR] Missing required files:" -ForegroundColor Red
        $missing | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
        Write-Host "`nMake sure these files are in: $ScriptDir" -ForegroundColor Yellow
        exit 1
    }
}

# --- UI: Select executables ---
function Select-Executables {
    $selectedExes = [System.Collections.Generic.List[string]]::new()

    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "  Windows Server Bypass Installer" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "`nThis tool makes Windows Server 2025 report as Windows 11 24H2"
    Write-Host "Workstation to selected applications.`n"

    # Preset selection
    Write-Host "--- Presets ---" -ForegroundColor Yellow
    $presetIndex = 1
    $presetKeys = @($Presets.Keys)
    foreach ($name in $presetKeys) {
        $exeList = $Presets[$name] -join ", "
        Write-Host "  [$presetIndex] $name ($exeList)"
        $presetIndex++
    }
    Write-Host ""

    foreach ($name in $presetKeys) {
        $answer = Read-Host "Include $name preset? (Y/n)"
        if ($answer -eq "" -or $answer -match "^[Yy]") {
            foreach ($exe in $Presets[$name]) {
                if (-not $selectedExes.Contains($exe)) {
                    $selectedExes.Add($exe)
                    Write-Host "    + $exe" -ForegroundColor Green
                }
            }
        }
    }

    # Custom executables
    Write-Host "`n--- Custom Executables ---" -ForegroundColor Yellow
    Write-Host "Enter additional executable names (e.g. myapp.exe), one per line."
    Write-Host "Press Enter on an empty line when done.`n"

    while ($true) {
        $custom = Read-Host "Add exe (blank to finish)"
        if ([string]::IsNullOrWhiteSpace($custom)) { break }

        $custom = $custom.Trim()
        if (-not $custom.EndsWith(".exe", [System.StringComparison]::OrdinalIgnoreCase)) {
            $custom += ".exe"
        }

        if ($selectedExes.Contains($custom)) {
            Write-Host "    Already in list: $custom" -ForegroundColor Yellow
        } else {
            $selectedExes.Add($custom)
            Write-Host "    + $custom" -ForegroundColor Green
        }
    }

    if ($selectedExes.Count -eq 0) {
        Write-Host "`n[ERROR] No executables selected. Nothing to install." -ForegroundColor Red
        exit 1
    }

    Write-Host "`n--- Selected Targets ---" -ForegroundColor Cyan
    $selectedExes | ForEach-Object { Write-Host "  * $_" -ForegroundColor White }

    return $selectedExes
}

# --- Generate Sprint Script ---
function New-SprintScript {
    param([System.Collections.Generic.List[string]]$Executables)

    $sb = [System.Text.StringBuilder]::new()

    # Header
    [void]$sb.AppendLine('// Auto-generated by Install.ps1')
    [void]$sb.AppendLine('call apphelp.dll!SdbCreateDatabase /return native /into pdb (lpwstr "Win11VersionLie.sdb", int 0)')
    [void]$sb.AppendLine('call apphelp.dll!SdbBeginWriteListTag /return int /into tDatabase (slotdata pdb, int 0x7001)')
    [void]$sb.AppendLine('call apphelp.dll!SdbWriteStringTag (slotdata pdb, int 0x6001, lpwstr "Win11VersionLie Database")')
    [void]$sb.AppendLine('call apphelp.dll!SdbWriteBinaryTag (slotdata pdb, int 0x9007, lpstr "Win11VerLieDBSdb", int 0x10)')
    [void]$sb.AppendLine('call apphelp.dll!SdbWriteQWORDTag (slotdata pdb, int 0x5001, long 0)')
    [void]$sb.AppendLine('call apphelp.dll!SdbWriteStringTag (slotdata pdb, int 0x6022, lpwstr "3.0.0.9")')
    [void]$sb.AppendLine('call apphelp.dll!SdbWriteDWORDTag (slotdata pdb, int 0x4021, int 39)')
    [void]$sb.AppendLine('call apphelp.dll!SdbWriteDWORDTag (slotdata pdb, int 0x4055, int 0)')

    # Library
    [void]$sb.AppendLine('call apphelp.dll!SdbBeginWriteListTag /return int /into tLibrary (slotdata pdb, int 0x7002)')
    [void]$sb.AppendLine('call apphelp.dll!SdbBeginWriteListTag /return int /into tShim1 (slotdata pdb, int 0x7004)')
    [void]$sb.AppendLine('call apphelp.dll!SdbWriteStringTag (slotdata pdb, int 0x6001, lpwstr "Win11VersionLie")')
    [void]$sb.AppendLine('call apphelp.dll!SdbWriteStringTag (slotdata pdb, int 0x600A, lpwstr "AcRes.dll")')
    [void]$sb.AppendLine('call apphelp.dll!SdbWriteBinaryTag (slotdata pdb, int 0x9010, lpstr "Win11VerLieShimS", int 0x10)')
    [void]$sb.AppendLine('call apphelp.dll!SdbWriteDWORDTag (slotdata pdb, int 0x4021, int 39)')
    [void]$sb.AppendLine('call apphelp.dll!SdbWriteNULLTag (slotdata pdb, int 0x1002)')
    [void]$sb.AppendLine('call apphelp.dll!SdbEndWriteListTag (slotdata pdb, slotdata tShim1)')
    [void]$sb.AppendLine('call apphelp.dll!SdbEndWriteListTag (slotdata pdb, slotdata tLibrary)')

    # EXE entries
    $i = 1
    foreach ($exe in $Executables) {
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($exe)
        # Pad/truncate the binary tag IDs to exactly 16 chars
        $exeTag = ($baseName + "ExeW11VerLie0000").Substring(0, 16)
        $appTag = ($baseName + "AppW11VerLie0000").Substring(0, 16)

        [void]$sb.AppendLine("call apphelp.dll!SdbBeginWriteListTag /return int /into tExe$i (slotdata pdb, int 0x7007)")
        [void]$sb.AppendLine("call apphelp.dll!SdbWriteStringTag (slotdata pdb, int 0x6001, lpwstr `"$exe`")")
        [void]$sb.AppendLine("call apphelp.dll!SdbWriteStringTag (slotdata pdb, int 0x6006, lpwstr `"$baseName`")")
        [void]$sb.AppendLine("call apphelp.dll!SdbWriteStringTag (slotdata pdb, int 0x6005, lpwstr `"Win11VersionLie`")")
        [void]$sb.AppendLine("call apphelp.dll!SdbWriteBinaryTag (slotdata pdb, int 0x9004, lpstr `"$exeTag`", int 0x10)")
        [void]$sb.AppendLine("call apphelp.dll!SdbWriteBinaryTag (slotdata pdb, int 0x9011, lpstr `"$appTag`", int 0x10)")
        [void]$sb.AppendLine("call apphelp.dll!SdbWriteDWORDTag (slotdata pdb, int 0x4021, int 39)")
        [void]$sb.AppendLine("call apphelp.dll!SdbBeginWriteListTag /return int /into tMatch$i (slotdata pdb, int 0x7008)")
        [void]$sb.AppendLine('call apphelp.dll!SdbWriteStringTag (slotdata pdb, int 0x6001, lpwstr "*")')
        [void]$sb.AppendLine("call apphelp.dll!SdbEndWriteListTag (slotdata pdb, slotdata tMatch$i)")
        [void]$sb.AppendLine("call apphelp.dll!SdbBeginWriteListTag /return int /into tRef$i (slotdata pdb, int 0x7009)")
        [void]$sb.AppendLine('call apphelp.dll!SdbWriteStringTag (slotdata pdb, int 0x6001, lpwstr "Win11VersionLie")')
        [void]$sb.AppendLine("call apphelp.dll!SdbEndWriteListTag (slotdata pdb, slotdata tRef$i)")
        [void]$sb.AppendLine("call apphelp.dll!SdbEndWriteListTag (slotdata pdb, slotdata tExe$i)")
        $i++
    }

    # Footer
    [void]$sb.AppendLine('call apphelp.dll!SdbEndWriteListTag (slotdata pdb, slotdata tDatabase)')
    [void]$sb.AppendLine('call apphelp.dll!SdbCloseDatabaseWrite (slotdata pdb)')

    $sb.ToString() | Set-Content -Path $SprintScript -Encoding UTF8 -NoNewline
    Write-Host "`n[OK] Generated sprint script with $($Executables.Count) exe target(s)" -ForegroundColor Green
}

# --- Uninstall previous SDB ---
function Remove-OldSdb {
    Write-Host "`n--- Removing Previous Installation ---" -ForegroundColor Yellow

    # Search registry for installed SDBs matching our name
    $sdbRegPath = "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\InstalledSDB"
    if (Test-Path $sdbRegPath) {
        Get-ChildItem $sdbRegPath -ErrorAction SilentlyContinue | ForEach-Object {
            $desc = (Get-ItemProperty $_.PSPath -ErrorAction SilentlyContinue).DatabaseDescription
            $dbPath = (Get-ItemProperty $_.PSPath -ErrorAction SilentlyContinue).DatabasePath
            if ($desc -like "*Win11VersionLie*" -or ($dbPath -and $dbPath -like "*Win11VersionLie*")) {
                Write-Host "  Uninstalling: $desc" -ForegroundColor Yellow
                if ($dbPath -and (Test-Path $dbPath)) {
                    & sdbinst.exe -u $dbPath 2>$null
                } else {
                    # Try uninstall by GUID
                    $guid = $_.PSChildName
                    & sdbinst.exe -u -g $guid 2>$null
                }
            }
        }
    }

    # Also try direct uninstall of the SDB file if it exists in the script dir
    if (Test-Path $SdbFile) {
        & sdbinst.exe -u $SdbFile 2>$null
    }

    Write-Host "  Done." -ForegroundColor Green
}

# --- Backup & copy AcRes.dll ---
function Install-AcResDll {
    Write-Host "`n--- Installing AcRes.dll ---" -ForegroundColor Yellow

    # Backup original if not already backed up
    if ((Test-Path $Sys32AcRes) -and -not (Test-Path $Sys32AcResOrig)) {
        Write-Host "  Backing up original AcRes.dll -> AcRes.orig" -ForegroundColor Yellow
        Copy-Item $Sys32AcRes $Sys32AcResOrig -Force
        Write-Host "  Backup saved: $Sys32AcResOrig" -ForegroundColor Green
    } elseif (Test-Path $Sys32AcResOrig) {
        Write-Host "  Backup already exists: $Sys32AcResOrig" -ForegroundColor Green
    } else {
        Write-Host "  No existing AcRes.dll to backup (first install)" -ForegroundColor Yellow
    }

    # Copy our custom DLL
    Write-Host "  Copying shim DLL to System32..." -ForegroundColor Yellow
    Copy-Item $AcResDll $Sys32AcRes -Force
    Write-Host "  Installed: $Sys32AcRes" -ForegroundColor Green
}

# --- Generate SDB and install ---
function Install-Sdb {
    Write-Host "`n--- Generating & Installing SDB ---" -ForegroundColor Yellow

    # Generate SDB from sprint script
    Push-Location $ScriptDir
    try {
        Write-Host "  Running SprintDLL..." -ForegroundColor Yellow
        & ".\SprintDLL.exe" run ".\Win11VersionLie.sprint"
        if (-not (Test-Path $SdbFile)) {
            Write-Host "  [ERROR] SDB file was not created!" -ForegroundColor Red
            exit 1
        }
        Write-Host "  Generated: $SdbFile" -ForegroundColor Green

        # Install SDB
        Write-Host "  Installing SDB..." -ForegroundColor Yellow
        & sdbinst.exe $SdbFile
        Write-Host "  SDB installed successfully!" -ForegroundColor Green
    } finally {
        Pop-Location
    }
}

# --- Uninstall ---
function Invoke-Uninstall {
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "  Windows Server Bypass - Uninstall" -ForegroundColor Cyan
    Write-Host "========================================`n" -ForegroundColor Cyan

    Remove-OldSdb

    if (Test-Path $Sys32AcResOrig) {
        Write-Host "`n--- Restoring Original AcRes.dll ---" -ForegroundColor Yellow
        Copy-Item $Sys32AcResOrig $Sys32AcRes -Force
        Remove-Item $Sys32AcResOrig -Force
        Write-Host "  Restored original AcRes.dll" -ForegroundColor Green
    }

    Write-Host "`nUninstall complete!`n" -ForegroundColor Cyan
}

# ===== Main =====
$action = $args[0]

if ($action -eq "-uninstall") {
    Invoke-Uninstall
    exit 0
}

Test-RequiredFiles
$exeList = Select-Executables

Write-Host ""
$confirm = Read-Host "Proceed with installation? (Y/n)"
if ($confirm -match "^[Nn]") {
    Write-Host "Cancelled." -ForegroundColor Yellow
    exit 0
}

Remove-OldSdb
New-SprintScript -Executables $exeList
Install-AcResDll
Install-Sdb

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  Installation Complete!" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "`nThe following apps will now see Windows 11 24H2 Workstation:" -ForegroundColor Green
$exeList | ForEach-Object { Write-Host "  * $_" -ForegroundColor White }
Write-Host "`nTo uninstall: double-click Uninstall.cmd (or run .\Install.ps1 -uninstall)" -ForegroundColor Yellow
Write-Host "To add/change apps: double-click Install.cmd again`n" -ForegroundColor Yellow

& timeout /t 30