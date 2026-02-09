#pragma once
#include "Shim.h"

// Win11VersionLie intercepts OS version-checking APIs to report Windows 11 24H2 Workstation
// instead of Windows Server 2025. Both share kernel 10.0.26100 but differ in product type.
// This makes apps that reject Server editions (e.g. Call of Duty) work on Server 2025.

class Shim_Win11VersionLie : public Shim {
public:
	Shim_Win11VersionLie();
protected:
	virtual void RegisterHooks();
private:
	static DWORD WINAPI Hook_GetVersion();
	static BOOL WINAPI Hook_GetVersionExA(LPOSVERSIONINFOA lpVersionInformation);
	static BOOL WINAPI Hook_GetVersionExW(LPOSVERSIONINFOW lpVersionInformation);
	static BOOL WINAPI Hook_VerifyVersionInfoW(LPOSVERSIONINFOEXW lpVersionInformation, DWORD dwTypeMask, DWORDLONG dwlConditionMask);
	static LONG WINAPI Hook_RtlGetVersion(LPOSVERSIONINFOW lpVersionInformation);
};
