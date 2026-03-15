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
	static BOOL WINAPI Hook_GetProductInfo(DWORD dwOSMajorVersion, DWORD dwOSMinorVersion, DWORD dwSpMajorVersion, DWORD dwSpMinorVersion, PDWORD pdwReturnedProductType);
	static LONG WINAPI Hook_RegGetValueW(HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData);
	static LONG WINAPI Hook_RegGetValueA(HKEY hkey, LPCSTR lpSubKey, LPCSTR lpValue, DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData);
	static LONG WINAPI Hook_RegQueryValueExW(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
	static LONG WINAPI Hook_RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
	static DWORD WINAPI Hook_SHGetValueW(HKEY hkey, LPCWSTR pszSubKey, LPCWSTR pszValue, DWORD* pdwType, void* pvData, DWORD* pcbData);
	static DWORD WINAPI Hook_SHGetValueA(HKEY hkey, LPCSTR pszSubKey, LPCSTR pszValue, DWORD* pdwType, void* pvData, DWORD* pcbData);
	static BOOL WINAPI Hook_IsOS(DWORD dwOS);
	static LONG WINAPI Hook_NtQueryValueKey(HANDLE KeyHandle, PVOID ValueName, ULONG KeyValueInformationClass, PVOID KeyValueInformation, ULONG Length, PULONG ResultLength);
};
