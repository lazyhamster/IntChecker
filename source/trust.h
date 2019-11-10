#ifndef trust_h__
#define trust_h__

enum VerificationResult
{
	VR_UNKNOWN,
	VR_SIGNATURE_VALID,
	VR_NO_SIGNATURE,
	VR_SIGNATURE_UNTRUSTED,
	VR_SIGNATURE_NOTALLOWED,
	VR_FILE_ERROR,
	VR_PROVIDER_UNKNOWN,
	VR_INVALID_SIGNATURE
};

bool FileCanHaveSignature(const wchar_t* path);
VerificationResult VerifyPeSignature(const wchar_t* path);

#endif // trust_h__
