# Windows Server Bypass

A shim-based solution that makes applications think they're running on **Windows 11 24H2 Workstation** instead of **Windows Server 2025**.

Windows Server 2025 and Windows 11 24H2 share the same kernel version (`10.0.26100`) — the only difference apps can see is the **product type** (`VER_NT_SERVER` vs `VER_NT_WORKSTATION`). Some applications, like Call of Duty, refuse to run on Server editions despite being fully compatible. This shim intercepts all OS version-checking APIs and reports Workstation instead of Server.

## Quick Start

1. Double-click **`Install.cmd`**
2. Accept the UAC prompt
3. Select the Call of Duty preset (or add custom executables)
4. Done! Launch your game.

## What It Does

The shim hooks 5 Windows APIs to spoof version info:

| API | DLL | What It Does |
|---|---|---|
| `GetVersion` | KERNEL32 | Returns packed Win11 24H2 DWORD |
| `GetVersionExA` | KERNEL32 | Overwrites version struct with Win11 Workstation |
| `GetVersionExW` | KERNEL32 | Same as above (wide-char variant) |
| `VerifyVersionInfoW` | KERNEL32 | Intercepts `IsWindowsServer()` checks |
| `RtlGetVersion` | NTDLL | Hooks the low-level call that bypasses compat shims |

### Spoofed Values

| Field | Value |
|---|---|
| Major.Minor.Build | 10.0.26100 |
| Product Type | `VER_NT_WORKSTATION` (1) |
| Platform ID | `VER_PLATFORM_WIN32_NT` (2) |
| Suite Mask | `VER_SUITE_SINGLEUSERTS` (0x100) |

## Presets

| Preset | Executables |
|---|---|
| Call of Duty | `cod.exe`, `cod24-cod.exe`, `cod25-cod.exe` |
| Oculus/Meta | `OculusSetup.exe`, `OculusClient.exe`, `OVRServer_x64.exe`, `OVRRedir.exe`, `OVRServiceLauncher.exe`, `OculusDash.exe`, `OculusDebugTool.exe`, `OVRTray.exe`, `OVRUpdate.exe`, `MetaQuestLink.exe`, `OculusLinkCable.exe`, `AirLinkLauncher.exe`, `OculusDiagnostics.exe`, `OVRPlatformSampleApp.exe`, `OculusWorldDemo.exe`, `OculusRoomTiny.exe`, `OVRPlugin.exe`, `OculusInstaller.exe`, `OVRMonitorConsole.exe`, `OculusPlatformUtil.exe`, `OVRLauncher.exe`, `OculusXRPlugin.exe`, `OVRServiceHost.exe`, `OculusKeepalive.exe`, `OVRErrorHandler.exe`, `OculusDashHelper.exe`, `OVRTracking.exe`, `OculusLogger.exe`, `OVRHeadsetProximity.exe`, `OculusBrowser.exe` |

You can add any additional executables during installation.

## Uninstall

Double-click **`Uninstall.cmd`** (or run `.\Install.ps1 -uninstall` from an admin PowerShell).

This will:
- Remove the installed SDB shim database
- Restore the original `AcRes.dll` from the backup (`AcRes.orig`)

## Files

| File | Purpose |
|---|---|
| `Install.cmd` | Double-click to install |
| `Uninstall.cmd` | Double-click to uninstall |
| `Install.ps1` | PowerShell installer script (called by the .cmd files) |
| `AcRes.dll` | The compiled custom shim DLL |
| `SprintDLL.exe` | Tool to generate SDB database files |
| `VersionCheck.exe` | Test tool to verify what version APIs are reporting |

## Testing

Run `VersionCheck.exe` after installation to verify the shim is working. You should see:

- `GetVersion`, `GetVersionExA/W`: reporting `10.0.26100` with `VER_NT_WORKSTATION`
- `RtlGetVersion`: also reporting `VER_NT_WORKSTATION` (hooked)
- `IsWindowsServer`: `FALSE`

## Re-configuring

To add or remove target executables, just double-click `Install.cmd` again. It automatically removes the previous installation before applying the new configuration.

## Building From Source

The shim source is in the `CustomShimRepo` directory. To build:

1. Open `CustomShimRepo/CustomShim/CustomShim.sln` in Visual Studio
2. Build **Release | x64**
3. The output DLL is your new `AcRes.dll`

## Requirements

- Windows Server 2025 (build 26100)
- Administrator privileges (for installing the SDB and copying to System32)
- PowerShell 5.1+
