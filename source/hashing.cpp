#include "stdafx.h"
#include "hashing.h"
#include "Utils.h"

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

int GenerateHash( const wchar_t* filePath, rhash_ids hashAlgo, char* result, HashingProgressFunc progressFunc, HANDLE progressContext )
{
	HANDLE hFile = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
	if (hFile == INVALID_HANDLE_VALUE) return GENERATE_ERROR;

	const size_t readBufSize = 32 * 1024;
	char readBuf[32 * 1024];
	DWORD numReadBytes;

	int retVal = GENERATE_SUCCESS;

	rhash hashCtx = rhash_init(hashAlgo);
	while (retVal == GENERATE_SUCCESS)
	{
		if (!ReadFile(hFile, readBuf, readBufSize, &numReadBytes, NULL) || !numReadBytes)
		{
			retVal = GENERATE_ERROR;
			break;
		}

		rhash_update(hashCtx, readBuf, numReadBytes);

		if (progressFunc != NULL)
		{
			if (!progressFunc(progressContext, numReadBytes))
				retVal = GENERATE_ABORTED;
		}
	}
	
	if (retVal == GENERATE_SUCCESS)
	{
		rhash_final(hashCtx, NULL);
		rhash_print(result, hashCtx, hashAlgo, RHPR_HEX);
	}

	rhash_free(hashCtx);
	CloseHandle(hFile);
	return retVal;
}

static wstring GetFullPath(const wchar_t* path)
{
	wchar_t tmpBuf[4096];
	GetFullPathName(path, ARRAY_SIZE(tmpBuf), tmpBuf, NULL);
	return tmpBuf;
}

static int EnumFiles(const wstring& baseAbsPath, const wstring& pathPrefix, StringList &destList, bool recursive)
{
	wstring strBasePath = baseAbsPath + L"*.*";

	WIN32_FIND_DATA fd;
	HANDLE hFind;
	int numFound = 0;

	hFind = FindFirstFile(strBasePath.c_str(), &fd);
	if (hFind == INVALID_HANDLE_VALUE) return 0;

	do 
	{
		if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
			continue;

		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 && recursive)
		{
			wstring strNexBasePath = baseAbsPath + fd.cFileName;
			strNexBasePath += L"\\";
			wstring strNextPrefix = pathPrefix + fd.cFileName;
			strNextPrefix += L"\\";
			
			numFound += EnumFiles(strNexBasePath, strNextPrefix, destList, recursive);
		}
		else
		{
			destList.push_back(pathPrefix + fd.cFileName);
			numFound++;
		}
	} while (FindNextFile(hFind, &fd));
	
	FindClose(hFind);
	return numFound;
}

int PrepareFilesList(const wchar_t* basePath, StringList &destList, bool recursive)
{
	wstring strBasePath = GetFullPath(basePath);
	wstring strStartPrefix(L"");
	IncludeTrailingPathDelim(strBasePath);
	
	return EnumFiles(strBasePath, strStartPrefix, destList, recursive);
}
