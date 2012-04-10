#include "stdafx.h"
#include "hashing.h"

Hasher::Hasher( const wchar_t* basePath, rhash_ids hashId )
{
	m_BasePath = normalizePath(basePath);
	m_HashId = hashId;
}

std::wstring Hasher::normalizePath( const wchar_t* input )
{
	DWORD absPathSize = GetFullPathName(input, 0, NULL, NULL) + 1;
	wchar_t* tmpBuf = (wchar_t*) malloc(absPathSize * sizeof(wchar_t));
	GetFullPathName(input, absPathSize, tmpBuf, NULL);
	
	std::wstring retVal(tmpBuf);
	free(tmpBuf);

	if (retVal[retVal.length() - 1] != '\\')
		retVal += '\\';

	return retVal;
}

bool Hasher::SaveList( const wchar_t* filepath )
{
	return false;
}

int Hasher::LoadList( const wchar_t* filepath )
{
	return -1;
}
