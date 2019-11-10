#include "stdafx.h"
#include "trust.h"

#include <WinTrust.h>
#include <SoftPub.h>

bool FileCanHaveSignature(const wchar_t* path)
{
	const wchar_t* ext = PathFindExtension(path);
	return (_wcsicmp(ext, L".exe") == 0);
}

VerificationResult VerifyPeSignature(const wchar_t* path)
{
	WINTRUST_FILE_INFO wtfi = { 0 };
	wtfi.cbStruct = sizeof(WINTRUST_FILE_INFO);
	wtfi.pcwszFilePath = path;

	WINTRUST_DATA wtData = { 0 };
	wtData.cbStruct = sizeof(WINTRUST_DATA);
	wtData.dwUIChoice = WTD_UI_NONE;
	wtData.dwUnionChoice = WTD_CHOICE_FILE;
	wtData.dwStateAction = WTD_STATEACTION_VERIFY;
	wtData.fdwRevocationChecks = WTD_REVOKE_NONE;
	wtData.pFile = &wtfi;

	GUID pgActionId = WINTRUST_ACTION_GENERIC_VERIFY_V2;

	LONG lStatus = WinVerifyTrust(NULL, &pgActionId, &wtData);

	VerificationResult vrResult = VR_UNKNOWN;
	switch (lStatus)
	{
	case ERROR_SUCCESS:
		vrResult = VR_SIGNATURE_VALID;
		break;
	case TRUST_E_NOSIGNATURE:
		vrResult = VR_NO_SIGNATURE;
		break;
	case TRUST_E_EXPLICIT_DISTRUST:
		vrResult = VR_SIGNATURE_NOTALLOWED;
		break;
	case TRUST_E_SUBJECT_NOT_TRUSTED:
		vrResult = VR_SIGNATURE_UNTRUSTED;
		break;
	case CRYPT_E_SECURITY_SETTINGS:
		break;
	case CRYPT_E_FILE_ERROR:
		vrResult = VR_FILE_ERROR;
		break;
	case TRUST_E_PROVIDER_UNKNOWN:
		vrResult = VR_PROVIDER_UNKNOWN;
		break;
	case TRUST_E_BAD_DIGEST:
		vrResult = VR_INVALID_SIGNATURE;
		break;
	default:
		break;
	}

	if (lStatus == ERROR_SUCCESS)
	{
		//TODO: get certificate info
	}

	wtData.dwStateAction = WTD_STATEACTION_CLOSE;
	WinVerifyTrust(0, &pgActionId, &wtData);

	return vrResult;
}
