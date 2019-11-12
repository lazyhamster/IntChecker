#ifndef trust_h__
#define trust_h__

struct CertificateInfo
{
	std::wstring IssuerName;
	std::wstring SubjectName;
	std::wstring SignatureAlgorithm;
};

struct DigitalSignatureInfo
{
	std::wstring ProgramName;
	std::wstring PublisherLink;
	std::wstring MoreInfoLink;
	
	CertificateInfo CertInfo;
};

bool FileCanHaveSignature(const wchar_t* path);
long VerifyPeSignature(const wchar_t* path);

#endif // trust_h__
