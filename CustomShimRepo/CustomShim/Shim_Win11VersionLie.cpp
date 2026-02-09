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
