#include "stdafx.h"
#include "hashing.h"

bool HashList::SaveList( const wchar_t* filepath )
{
	return false;
}

int HashList::LoadList( const wchar_t* filepath, bool replaceExisting /*= true*/ )
{
	return 0;
}

std::string HashList::GetFileHash( const wchar_t* FileName ) const
{
	/*
	vector<FileHashInfo>::const_iterator cit;
	for (cit = m_HashList.begin(); cit != m_HashList.end(); cit++)
	{
		FileHashInfo info = *cit;
		if (_wcsicmp(info.Filename.c_str(), FileName) == 0)
			return info.HashStr;
	}
	*/

	return "";
}

void HashList::SetFileHash( const wchar_t* FileName, std::string HashVal )
{
	//
}

//////////////////////////////////////////////////////////////////////////

static int64_t GetFileSize_i64(HANDLE hFile)
{
	LARGE_INTEGER liSize;
	return GetFileSizeEx(hFile, &liSize) ? liSize.QuadPart : -1;
}

bool GenerateHash( const wchar_t* filePath, rhash_ids hashAlgo, char* result, HashingProgressFunc progressFunc, HANDLE progressContext )
{
	HANDLE hFile = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
	if (hFile == INVALID_HANDLE_VALUE) return false;

	const size_t readBufSize = 32 * 1024;
	char readBuf[32 * 1024];
	DWORD numReadBytes;

	bool retVal = true;
	int64_t totalBytesProcessed = 0, totalFileSize = GetFileSize_i64(hFile);

	rhash hashCtx = rhash_init(hashAlgo);
	while (retVal)
	{
		retVal = ReadFile(hFile, readBuf, readBufSize, &numReadBytes, NULL) != FALSE;
		if (!retVal || !numReadBytes) break;

		rhash_update(hashCtx, readBuf, numReadBytes);

		totalBytesProcessed += numReadBytes;
		if (progressFunc != NULL)
		{
			progressFunc(progressContext, totalBytesProcessed, totalFileSize);
		}
	}
	rhash_final(hashCtx, NULL);
	rhash_print(result, hashCtx, hashAlgo, RHPR_HEX);

	rhash_free(hashCtx);
	CloseHandle(hFile);
	return retVal;
}
