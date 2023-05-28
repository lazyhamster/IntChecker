#include "stdafx.h"
#include "trust.h"

#include <WinTrust.h>
#include <wincrypt.h>
#include <SoftPub.h>

static std::wstring GetAlgorithmName(const CRYPT_ALGORITHM_IDENTIFIER &algoId)
{
	if (algoId.pszObjId)
	{
		PCCRYPT_OID_INFO pOidInfo = CryptFindOIDInfo(CRYPT_OID_INFO_OID_KEY, algoId.pszObjId, 0);
		if (pOidInfo && pOidInfo->pwszName)
			return pOidInfo->pwszName;
	}

	return L"";
}

static bool GetCertificateInfo(HCERTSTORE hStore, PCMSG_SIGNER_INFO pSignerInfo, CertificateInfo& decodedInfo)
{
	CERT_INFO certInfo = {0};
	certInfo.Issuer = pSignerInfo->Issuer;
	certInfo.SerialNumber = pSignerInfo->SerialNumber;

	PCCERT_CONTEXT pCertContext = CertFindCertificateInStore(hStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_SUBJECT_CERT, &certInfo, NULL);
	if (!pCertContext) return false;

	wchar_t wszNameBuf[1024] = { 0 };
	DWORD dwRetSize;

	dwRetSize = CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, CERT_NAME_ISSUER_FLAG, NULL, wszNameBuf, _countof(wszNameBuf));
	decodedInfo.IssuerName = dwRetSize > 1 ? wszNameBuf : L"";

	dwRetSize = CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, wszNameBuf, _countof(wszNameBuf));
	decodedInfo.SubjectName = dwRetSize > 1 ? wszNameBuf : L"";

	decodedInfo.SignatureAlgorithm = pCertContext->pCertInfo ? GetAlgorithmName(pCertContext->pCertInfo->SignatureAlgorithm) : L"";

	CertFreeCertificateContext(pCertContext);
	return true;
}

static bool GetProgAndPublisherInfo(PCMSG_SIGNER_INFO pSignerInfo, DigitalSignatureInfo &sigInfo)
{
	for (DWORD i = 0; i < pSignerInfo->AuthAttrs.cAttr; ++i)
	{
		if (strcmp(pSignerInfo->AuthAttrs.rgAttr[i].pszObjId, SPC_SP_OPUS_INFO_OBJID) == 0)
		{
			PSPC_SP_OPUS_INFO pOpusInfo = NULL;
			DWORD dwDataSize = 0;

			if (CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, SPC_SP_OPUS_INFO_OBJID,
				pSignerInfo->AuthAttrs.rgAttr[i].rgValue[0].pbData, pSignerInfo->AuthAttrs.rgAttr[i].rgValue[0].cbData,
				CRYPT_DECODE_ALLOC_FLAG, 0, &pOpusInfo, &dwDataSize) && dwDataSize && pOpusInfo)
			{
				if (pOpusInfo->pwszProgramName)
				{
					sigInfo.ProgramName = pOpusInfo->pwszProgramName;
				}
				if (pOpusInfo->pPublisherInfo)
				{
					switch (pOpusInfo->pPublisherInfo->dwLinkChoice)
					{
					case SPC_URL_LINK_CHOICE:
						sigInfo.PublisherLink = pOpusInfo->pPublisherInfo->pwszUrl;
						break;
					case SPC_FILE_LINK_CHOICE:
						sigInfo.PublisherLink = pOpusInfo->pPublisherInfo->pwszFile;
						break;
					}
				}
				if (pOpusInfo->pMoreInfo)
				{
					switch (pOpusInfo->pMoreInfo->dwLinkChoice)
					{
					case SPC_URL_LINK_CHOICE:
						sigInfo.MoreInfoLink = pOpusInfo->pMoreInfo->pwszUrl;
						break;
					case SPC_FILE_LINK_CHOICE:
						sigInfo.MoreInfoLink = pOpusInfo->pMoreInfo->pwszFile;
						break;
					}
				}
			}

			if (pOpusInfo) LocalFree(pOpusInfo);

			return true;
		}
	}
	
	return false;
}

bool GetDigitalSignatureInfo(const wchar_t* path, SignedFileInformation &info)
{
	DWORD dwEncodingType, dwContentType, dwFormatType;
	HCERTSTORE hStore = nullptr;
	HCRYPTMSG hMsg = nullptr;

	BOOL fResult = CryptQueryObject(CERT_QUERY_OBJECT_FILE, path, CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
		CERT_QUERY_FORMAT_FLAG_BINARY, 0, &dwEncodingType, &dwContentType, &dwFormatType, &hStore, &hMsg, nullptr);
	if (!fResult) return false;

	DWORD dwNumSigners;
	DWORD dwParamSize = sizeof(dwNumSigners);

	fResult = CryptMsgGetParam(hMsg, CMSG_SIGNER_COUNT_PARAM, 0, &dwNumSigners, &dwParamSize);
	if (fResult)
	{
		for (DWORD i = 0; i < dwNumSigners; ++i)
		{
			DWORD dwSignerInfoSize = 0;

			fResult = CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, i, nullptr, &dwSignerInfoSize);
			if (fResult)
			{
				PCMSG_SIGNER_INFO pSignerInfo = (PCMSG_SIGNER_INFO)LocalAlloc(LPTR, dwSignerInfoSize);
				fResult = CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, i, pSignerInfo, &dwSignerInfoSize);
				if (fResult)
				{
					DigitalSignatureInfo signatureInfo;
					if (GetCertificateInfo(hStore, pSignerInfo, signatureInfo.CertInfo)
						&& GetProgAndPublisherInfo(pSignerInfo, signatureInfo))
					{
						signatureInfo.HashAlgorithm = GetAlgorithmName(pSignerInfo->HashAlgorithm);
						signatureInfo.HashEncryptionAlgorithm = GetAlgorithmName(pSignerInfo->HashEncryptionAlgorithm);
						
						info.Signatures.push_back(signatureInfo);
					}
				}
				LocalFree(pSignerInfo);
			}
		}
	}

	if (hStore) CertCloseStore(hStore, 0);
	if (hMsg) CryptMsgClose(hMsg);

	return info.Signatures.size() > 0;
}

long VerifyPeSignature(const wchar_t* path)
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

	wtData.dwStateAction = WTD_STATEACTION_CLOSE;
	WinVerifyTrust(0, &pgActionId, &wtData);

	return lStatus;
}
