#include "Shim_Win11VersionLie.h"

// Win11VersionLie: Spoofs OS version to Windows 11 24H2 Workstation (10.0.26100)
// Hooks GetVersion, GetVersionExA, GetVersionExW, and VerifyVersionInfoW

SHIM_INSTANCE(Win11VersionLie)

// Spoofed version constants
static const DWORD FAKE_MAJOR = 10;
static const DWORD FAKE_MINOR = 0;
static const DWORD FAKE_BUILD = 26100;
static const DWORD FAKE_PLATFORMID = VER_PLATFORM_WIN32_NT;
static const BYTE  FAKE_PRODUCT_TYPE = VER_NT_WORKSTATION;
static const WORD  FAKE_SUITE_MASK = VER_SUITE_SINGLEUSERTS;
static const DWORD FAKE_GETVERSION = 0x65F4000A; // Packed: build 26100, minor 0, major 10

void Shim_Win11VersionLie::RegisterHooks() {
	ADD_HOOK("KERNEL32.DLL", "GetVersion", Hook_GetVersion);
	ADD_HOOK("KERNEL32.DLL", "GetVersionExA", Hook_GetVersionExA);
	ADD_HOOK("KERNEL32.DLL", "GetVersionExW", Hook_GetVersionExW);
	ADD_HOOK("KERNEL32.DLL", "VerifyVersionInfoW", Hook_VerifyVersionInfoW);
	ADD_HOOK("KERNEL32.DLL", "GetProductInfo", Hook_GetProductInfo);
	ADD_HOOK("ADVAPI32.DLL", "RegGetValueW", Hook_RegGetValueW);
	ADD_HOOK("ADVAPI32.DLL", "RegGetValueA", Hook_RegGetValueA);
	ADD_HOOK("ADVAPI32.DLL", "RegQueryValueExW", Hook_RegQueryValueExW);
	ADD_HOOK("ADVAPI32.DLL", "RegQueryValueExA", Hook_RegQueryValueExA);
	ADD_HOOK("SHLWAPI.DLL", "SHGetValueW", Hook_SHGetValueW);
	ADD_HOOK("SHLWAPI.DLL", "SHGetValueA", Hook_SHGetValueA);
	ADD_HOOK("SHLWAPI.DLL", "IsOS", Hook_IsOS);
	ADD_HOOK("NTDLL.DLL", "NtQueryValueKey", Hook_NtQueryValueKey);
	ADD_HOOK("NTDLL.DLL", "RtlGetVersion", Hook_RtlGetVersion);
}

DWORD WINAPI Shim_Win11VersionLie::Hook_GetVersion() {
	ASL_PRINTF(ASL_LEVEL_TRACE, "GetVersion -> returning 0x%08X (Win11 24H2)", FAKE_GETVERSION);
	return FAKE_GETVERSION;
}

BOOL WINAPI Shim_Win11VersionLie::Hook_GetVersionExA(LPOSVERSIONINFOA lpVersionInformation) {
	DEFINE_NEXT(Hook_GetVersionExA);
	BOOL result = next(lpVersionInformation);
	if (!result) return result;

	lpVersionInformation->dwMajorVersion = FAKE_MAJOR;
	lpVersionInformation->dwMinorVersion = FAKE_MINOR;
	lpVersionInformation->dwBuildNumber = FAKE_BUILD;
	lpVersionInformation->dwPlatformId = FAKE_PLATFORMID;
	lpVersionInformation->szCSDVersion[0] = '\0';

	// Check if caller passed OSVERSIONINFOEXA (extended structure)
	if (lpVersionInformation->dwOSVersionInfoSize == sizeof(OSVERSIONINFOEXA)) {
		LPOSVERSIONINFOEXA lpExA = (LPOSVERSIONINFOEXA)lpVersionInformation;
		lpExA->wServicePackMajor = 0;
		lpExA->wServicePackMinor = 0;
		lpExA->wProductType = FAKE_PRODUCT_TYPE;
		lpExA->wSuiteMask = FAKE_SUITE_MASK;
	}

	ASL_PRINTF(ASL_LEVEL_TRACE, "GetVersionExA -> spoofed to Win11 24H2 Workstation");
	return result;
}

BOOL WINAPI Shim_Win11VersionLie::Hook_GetVersionExW(LPOSVERSIONINFOW lpVersionInformation) {
	DEFINE_NEXT(Hook_GetVersionExW);
	BOOL result = next(lpVersionInformation);
	if (!result) return result;

	lpVersionInformation->dwMajorVersion = FAKE_MAJOR;
	lpVersionInformation->dwMinorVersion = FAKE_MINOR;
	lpVersionInformation->dwBuildNumber = FAKE_BUILD;
	lpVersionInformation->dwPlatformId = FAKE_PLATFORMID;
	lpVersionInformation->szCSDVersion[0] = L'\0';

	// Check if caller passed OSVERSIONINFOEXW (extended structure)
	if (lpVersionInformation->dwOSVersionInfoSize == sizeof(OSVERSIONINFOEXW)) {
		LPOSVERSIONINFOEXW lpExW = (LPOSVERSIONINFOEXW)lpVersionInformation;
		lpExW->wServicePackMajor = 0;
		lpExW->wServicePackMinor = 0;
		lpExW->wProductType = FAKE_PRODUCT_TYPE;
		lpExW->wSuiteMask = FAKE_SUITE_MASK;
	}

	ASL_PRINTF(ASL_LEVEL_TRACE, "GetVersionExW -> spoofed to Win11 24H2 Workstation");
	return result;
}

BOOL WINAPI Shim_Win11VersionLie::Hook_VerifyVersionInfoW(LPOSVERSIONINFOEXW lpVersionInformation, DWORD dwTypeMask, DWORDLONG dwlConditionMask) {
	DEFINE_NEXT(Hook_VerifyVersionInfoW);

	// VER_PRODUCT_TYPE = 0x00000080
	if (!(dwTypeMask & VER_PRODUCT_TYPE)) {
		// No product type check requested, pass through
		return next(lpVersionInformation, dwTypeMask, dwlConditionMask);
	}

	LPOSVERSIONINFOEXW lpExW = lpVersionInformation;

	// Extract the condition for product type from the condition mask.
	// VerifyVersionInfo condition mask encodes conditions in 3-bit groups.
	// For VER_PRODUCT_TYPE, the condition bits are at positions 21-23 (bit position = type_bit_index * 3).
	// VER_PRODUCT_TYPE is bit 7 (0x80), so condition is at bits 7*3 = 21..23.
	BYTE productTypeCondition = (BYTE)((dwlConditionMask >> 21) & 0x07);
	BYTE requestedProductType = lpExW->wProductType;

	// Evaluate the condition against our fake product type
	BOOL productTypePass = FALSE;
	switch (productTypeCondition) {
	case VER_EQUAL:       // 1
		productTypePass = (FAKE_PRODUCT_TYPE == requestedProductType);
		break;
	case VER_GREATER:     // 2
		productTypePass = (FAKE_PRODUCT_TYPE > requestedProductType);
		break;
	case VER_GREATER_EQUAL: // 3
		productTypePass = (FAKE_PRODUCT_TYPE >= requestedProductType);
		break;
	case VER_LESS:        // 4
		productTypePass = (FAKE_PRODUCT_TYPE < requestedProductType);
		break;
	case VER_LESS_EQUAL:  // 5
		productTypePass = (FAKE_PRODUCT_TYPE <= requestedProductType);
		break;
	default:
		// Unknown condition, assume pass
		productTypePass = TRUE;
		break;
	}

	ASL_PRINTF(ASL_LEVEL_TRACE, "VerifyVersionInfoW: product type check condition=%d requested=%d fake=%d result=%d",
		productTypeCondition, requestedProductType, FAKE_PRODUCT_TYPE, productTypePass);

	// Strip VER_PRODUCT_TYPE from the type mask
	DWORD remainingTypeMask = dwTypeMask & ~VER_PRODUCT_TYPE;

	// Clear the product type condition bits (bits 21-23) from the condition mask
	DWORDLONG remainingConditionMask = dwlConditionMask & ~(0x07ULL << 21);

	if (remainingTypeMask == 0) {
		// Product type was the only check. Return our result.
		if (!productTypePass) {
			SetLastError(ERROR_OLD_WIN_VERSION);
		}
		return productTypePass;
	}

	// There are other fields to check. Call the real function for those.
	BOOL otherFieldsPass = next(lpVersionInformation, remainingTypeMask, remainingConditionMask);

	if (productTypePass && otherFieldsPass) {
		return TRUE;
	}

	// If either check failed, set the expected error code
	SetLastError(ERROR_OLD_WIN_VERSION);
	return FALSE;
}

BOOL WINAPI Shim_Win11VersionLie::Hook_GetProductInfo(DWORD dwOSMajorVersion, DWORD dwOSMinorVersion, DWORD dwSpMajorVersion, DWORD dwSpMinorVersion, PDWORD pdwReturnedProductType) {
	DEFINE_NEXT(Hook_GetProductInfo);
	BOOL result = next(dwOSMajorVersion, dwOSMinorVersion, dwSpMajorVersion, dwSpMinorVersion, pdwReturnedProductType);
	if (!result || !pdwReturnedProductType) return result;

	// PRODUCT_PROFESSIONAL = 0x30 (Windows 11 Pro workstation SKU)
	// Replace any server/unlicensed SKU so apps don't reject the OS edition.
	ASL_PRINTF(ASL_LEVEL_TRACE, "GetProductInfo -> replacing product type 0x%08X with PRODUCT_PROFESSIONAL", *pdwReturnedProductType);
	*pdwReturnedProductType = 0x00000030; // PRODUCT_PROFESSIONAL
	return result;
}

// Registry value names whose values identify the OS edition.
// We spoof these to look like a standard Windows 11 Pro workstation install.
struct RegSpoof { LPCWSTR name; LPCWSTR value; };
static const RegSpoof REG_SPOOFS[] = {
	{ L"EditionID",        L"Professional"  },  // "ServerDatacenter" -> "Professional"
	{ L"InstallationType", L"Client"        },  // "Server" -> "Client"
	{ L"ProductName",      L"Windows 11 Pro"},  // "Windows Server 2025 Datacenter" -> "Windows 11 Pro"
};

static LPCWSTR GetSpoofedRegValue(LPCWSTR valueName) {
	if (!valueName) return nullptr;
	for (const auto& s : REG_SPOOFS) {
		if (_wcsicmp(valueName, s.name) == 0)
			return s.value;
	}
	return nullptr;
}

// Writes a REG_SZ value into the output buffers, handling size-query (pvData==NULL) and
// ERROR_MORE_DATA correctly for both RegGetValueW and RegQueryValueExW callers.
static LONG WriteRegistrySZ(LPCWSTR value, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData) {
	DWORD needed = (DWORD)((wcslen(value) + 1) * sizeof(WCHAR));
	if (pdwType) *pdwType = REG_SZ;
	if (!pvData) {
		if (pcbData) *pcbData = needed;
		return ERROR_SUCCESS;
	}
	if (!pcbData || *pcbData < needed) {
		if (pcbData) *pcbData = needed;
		return ERROR_MORE_DATA;
	}
	*pcbData = needed;
	memcpy(pvData, value, needed);
	return ERROR_SUCCESS;
}

LONG WINAPI Shim_Win11VersionLie::Hook_RegGetValueW(HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData) {
	DEFINE_NEXT(Hook_RegGetValueW);
	LPCWSTR spoofed = GetSpoofedRegValue(lpValue);
	if (spoofed) {
		ASL_PRINTF(ASL_LEVEL_TRACE, "RegGetValueW: spoofing '%S' -> '%S'", lpValue, spoofed);
		return WriteRegistrySZ(spoofed, pdwType, pvData, pcbData);
	}
	return next(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
}

LONG WINAPI Shim_Win11VersionLie::Hook_RegQueryValueExW(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
	DEFINE_NEXT(Hook_RegQueryValueExW);
	LPCWSTR spoofed = GetSpoofedRegValue(lpValueName);
	if (spoofed) {
		ASL_PRINTF(ASL_LEVEL_TRACE, "RegQueryValueExW: spoofing '%S' -> '%S'", lpValueName, spoofed);
		return WriteRegistrySZ(spoofed, lpType, (PVOID)lpData, lpcbData);
	}
	return next(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
}

static LPCWSTR GetSpoofedRegValueA(LPCSTR valueName) {
	if (!valueName) return nullptr;
	if (_stricmp(valueName, "EditionID") == 0)        return L"Professional";
	if (_stricmp(valueName, "InstallationType") == 0) return L"Client";
	if (_stricmp(valueName, "ProductName") == 0)      return L"Windows 11 Pro";
	return nullptr;
}

// Like WriteRegistrySZ but converts the wide spoof value to ANSI for callers using A APIs.
static LONG WriteRegistrySZA(LPCWSTR value, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData) {
	int needed = WideCharToMultiByte(CP_ACP, 0, value, -1, nullptr, 0, nullptr, nullptr);
	if (pdwType) *pdwType = REG_SZ;
	if (!pvData) {
		if (pcbData) *pcbData = (DWORD)needed;
		return ERROR_SUCCESS;
	}
	if (!pcbData || *pcbData < (DWORD)needed) {
		if (pcbData) *pcbData = (DWORD)needed;
		return ERROR_MORE_DATA;
	}
	*pcbData = (DWORD)needed;
	WideCharToMultiByte(CP_ACP, 0, value, -1, (LPSTR)pvData, needed, nullptr, nullptr);
	return ERROR_SUCCESS;
}

LONG WINAPI Shim_Win11VersionLie::Hook_RegGetValueA(HKEY hkey, LPCSTR lpSubKey, LPCSTR lpValue, DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData) {
	DEFINE_NEXT(Hook_RegGetValueA);
	LPCWSTR spoofed = GetSpoofedRegValueA(lpValue);
	if (spoofed) {
		ASL_PRINTF(ASL_LEVEL_TRACE, "RegGetValueA: spoofing '%s' -> '%S'", lpValue, spoofed);
		return WriteRegistrySZA(spoofed, pdwType, pvData, pcbData);
	}
	return next(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
}

LONG WINAPI Shim_Win11VersionLie::Hook_RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
	DEFINE_NEXT(Hook_RegQueryValueExA);
	LPCWSTR spoofed = GetSpoofedRegValueA(lpValueName);
	if (spoofed) {
		ASL_PRINTF(ASL_LEVEL_TRACE, "RegQueryValueExA: spoofing '%s' -> '%S'", lpValueName, spoofed);
		return WriteRegistrySZA(spoofed, lpType, (PVOID)lpData, lpcbData);
	}
	return next(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
}

DWORD WINAPI Shim_Win11VersionLie::Hook_SHGetValueW(HKEY hkey, LPCWSTR pszSubKey, LPCWSTR pszValue, DWORD* pdwType, void* pvData, DWORD* pcbData) {
	DEFINE_NEXT(Hook_SHGetValueW);
	LPCWSTR spoofed = GetSpoofedRegValue(pszValue);
	if (spoofed) {
		ASL_PRINTF(ASL_LEVEL_TRACE, "SHGetValueW: spoofing '%S' -> '%S'", pszValue, spoofed);
		return WriteRegistrySZ(spoofed, pdwType, pvData, pcbData);
	}
	return next(hkey, pszSubKey, pszValue, pdwType, pvData, pcbData);
}

DWORD WINAPI Shim_Win11VersionLie::Hook_SHGetValueA(HKEY hkey, LPCSTR pszSubKey, LPCSTR pszValue, DWORD* pdwType, void* pvData, DWORD* pcbData) {
	DEFINE_NEXT(Hook_SHGetValueA);
	LPCWSTR spoofed = GetSpoofedRegValueA(pszValue);
	if (spoofed) {
		ASL_PRINTF(ASL_LEVEL_TRACE, "SHGetValueA: spoofing '%s' -> '%S'", pszValue, spoofed);
		return WriteRegistrySZA(spoofed, pdwType, pvData, pcbData);
	}
	return next(hkey, pszSubKey, pszValue, pdwType, pvData, pcbData);
}

// IsOS constants (from shlwapi.h)
#define OS_ANYSERVER     29  // TRUE on any Server edition
#define OS_TERMINALSERVER 18 // TRUE when Terminal Services is active (always true on Server)

BOOL WINAPI Shim_Win11VersionLie::Hook_IsOS(DWORD dwOS) {
	DEFINE_NEXT(Hook_IsOS);
	if (dwOS == OS_ANYSERVER || dwOS == OS_TERMINALSERVER) {
		ASL_PRINTF(ASL_LEVEL_TRACE, "IsOS(%lu) -> FALSE (spoofed)", dwOS);
		return FALSE;
	}
	return next(dwOS);
}

// Minimal NT types needed for NtQueryValueKey — avoids pulling in winternl.h/DDK headers.
struct NT_UNICODE_STRING { USHORT Length; USHORT MaximumLength; PWSTR Buffer; };
struct KEY_VALUE_PARTIAL_INFO { ULONG TitleIndex; ULONG Type; ULONG DataLength; UCHAR Data[1]; };
struct KEY_VALUE_FULL_INFO    { ULONG TitleIndex; ULONG Type; ULONG DataOffset; ULONG DataLength; ULONG NameLength; WCHAR Name[1]; };
#define NT_KV_FULL_INFO    1
#define NT_KV_PARTIAL_INFO 2
// NT status codes
#define STATUS_BUFFER_TOO_SMALL 0xC0000023L
#define STATUS_BUFFER_OVERFLOW  0x80000005L

static LPCWSTR GetSpoofedRegValueNt(NT_UNICODE_STRING* us) {
	if (!us || !us->Buffer || us->Length == 0) return nullptr;
	ULONG chars = us->Length / sizeof(WCHAR);
	for (const auto& s : REG_SPOOFS) {
		if (wcslen(s.name) == chars && _wcsnicmp(us->Buffer, s.name, chars) == 0)
			return s.value;
	}
	return nullptr;
}

LONG WINAPI Shim_Win11VersionLie::Hook_NtQueryValueKey(HANDLE KeyHandle, PVOID ValueName, ULONG KeyValueInformationClass, PVOID KeyValueInformation, ULONG Length, PULONG ResultLength) {
	DEFINE_NEXT(Hook_NtQueryValueKey);
	LONG status = next(KeyHandle, ValueName, KeyValueInformationClass, KeyValueInformation, Length, ResultLength);

	// Only intercept if the value was found (success or buffer-size responses).
	// Pass through genuine errors like STATUS_OBJECT_NAME_NOT_FOUND.
	if (status != 0 && status != STATUS_BUFFER_TOO_SMALL && status != STATUS_BUFFER_OVERFLOW)
		return status;

	LPCWSTR spoofed = GetSpoofedRegValueNt((NT_UNICODE_STRING*)ValueName);
	if (!spoofed) return status;

	ULONG spoofedBytes = (ULONG)((wcslen(spoofed) + 1) * sizeof(WCHAR));

	if (KeyValueInformationClass == NT_KV_PARTIAL_INFO) {
		ULONG needed = offsetof(KEY_VALUE_PARTIAL_INFO, Data) + spoofedBytes;
		if (ResultLength) *ResultLength = needed;
		if (Length < needed || !KeyValueInformation) return STATUS_BUFFER_TOO_SMALL;
		KEY_VALUE_PARTIAL_INFO* info = (KEY_VALUE_PARTIAL_INFO*)KeyValueInformation;
		info->Type = REG_SZ;
		info->DataLength = spoofedBytes;
		memcpy(info->Data, spoofed, spoofedBytes);
		ASL_PRINTF(ASL_LEVEL_TRACE, "NtQueryValueKey(Partial): spoofed '%.*S' -> '%S'", (int)(((NT_UNICODE_STRING*)ValueName)->Length / sizeof(WCHAR)), ((NT_UNICODE_STRING*)ValueName)->Buffer, spoofed);
		return 0;
	}

	if (KeyValueInformationClass == NT_KV_FULL_INFO && status == 0 && KeyValueInformation) {
		KEY_VALUE_FULL_INFO* info = (KEY_VALUE_FULL_INFO*)KeyValueInformation;
		ULONG needed = info->DataOffset + spoofedBytes;
		info->Type = REG_SZ;
		info->DataLength = spoofedBytes;
		if (ResultLength) *ResultLength = needed;
		if (Length >= needed) {
			memcpy((BYTE*)info + info->DataOffset, spoofed, spoofedBytes);
			ASL_PRINTF(ASL_LEVEL_TRACE, "NtQueryValueKey(Full): spoofed '%.*S' -> '%S'", (int)(((NT_UNICODE_STRING*)ValueName)->Length / sizeof(WCHAR)), ((NT_UNICODE_STRING*)ValueName)->Buffer, spoofed);
			return 0;
		}
		return STATUS_BUFFER_TOO_SMALL;
	}

	return status;
}

LONG WINAPI Shim_Win11VersionLie::Hook_RtlGetVersion(LPOSVERSIONINFOW lpVersionInformation) {
	DEFINE_NEXT(Hook_RtlGetVersion);
	LONG status = next(lpVersionInformation);
	if (status != 0) return status; // STATUS_SUCCESS == 0

	lpVersionInformation->dwMajorVersion = FAKE_MAJOR;
	lpVersionInformation->dwMinorVersion = FAKE_MINOR;
	lpVersionInformation->dwBuildNumber = FAKE_BUILD;
	lpVersionInformation->dwPlatformId = FAKE_PLATFORMID;
	lpVersionInformation->szCSDVersion[0] = L'\0';

	if (lpVersionInformation->dwOSVersionInfoSize == sizeof(OSVERSIONINFOEXW)) {
		LPOSVERSIONINFOEXW lpExW = (LPOSVERSIONINFOEXW)lpVersionInformation;
		lpExW->wServicePackMajor = 0;
		lpExW->wServicePackMinor = 0;
		lpExW->wProductType = FAKE_PRODUCT_TYPE;
		lpExW->wSuiteMask = FAKE_SUITE_MASK;
	}

	ASL_PRINTF(ASL_LEVEL_TRACE, "RtlGetVersion -> spoofed to Win11 24H2 Workstation");
	return status;
}
