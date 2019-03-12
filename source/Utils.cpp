#include "StdAfx.h"
#include "Utils.h"

///////////////////////////////////////////////////////////////////////////////
//							 Various routines								 //
///////////////////////////////////////////////////////////////////////////////

bool CheckEsc()
{
	DWORD dwNumEvents;
	_INPUT_RECORD inRec;

	HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
	if (GetNumberOfConsoleInputEvents(hConsole, &dwNumEvents))
		while (PeekConsoleInput(hConsole, &inRec, 1, &dwNumEvents) && (dwNumEvents > 0))
		{
			ReadConsoleInput(hConsole, &inRec, 1, &dwNumEvents);
			if ((inRec.EventType == KEY_EVENT) && (inRec.Event.KeyEvent.bKeyDown) && (inRec.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE))
				return true;
		} //while

		return false;
}

void IncludeTrailingPathDelim(wchar_t *pathBuf, size_t bufMaxSize)
{
	size_t nPathLen = wcslen(pathBuf);
	if ((nPathLen > 0) && (pathBuf[nPathLen - 1] != '\\'))
		wcscat_s(pathBuf, bufMaxSize, L"\\");
}

void IncludeTrailingPathDelim(wstring &pathStr)
{
	size_t nStrLen = pathStr.length();
	if ((nStrLen > 0) && (pathStr[nStrLen - 1] != '\\'))
		pathStr += L"\\";
}

int64_t GetFileSize_i64(const wchar_t* path)
{
	WIN32_FIND_DATA fd = {0};
	HANDLE hFind = FindFirstFile(path, &fd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		FindClose(hFind);
		return ((int64_t)fd.nFileSizeHigh << 32) + fd.nFileSizeLow;
	}

	return 0;
}

int64_t GetFileSize_i64(HANDLE hFile)
{
	LARGE_INTEGER sz;
	if (GetFileSizeEx(hFile, &sz))
		return sz.QuadPart;

	return 0;
}

bool IsFile(const std::wstring &path, int64_t *fileSize)
{
	std::wstring strSearchPath = PrependLongPrefix(path);

	WIN32_FIND_DATA fd = {0};
	HANDLE hFind = FindFirstFile(strSearchPath.c_str(), &fd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		FindClose(hFind);
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		{
			if (fileSize)
			{
				*fileSize = ((int64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
			}
			return true;
		}
	}

	return false;
}

bool CanCreateFile(const wchar_t* path)
{
	HANDLE hf = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, 0);
	if (hf != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hf);
		return true;
	}

	return false;
}

void TrimRight( char* str )
{
	size_t strLen = strlen(str);
	while (strLen > 0)
	{
		char lastChar = str[strLen - 1];
		if ((lastChar == '\n') || (lastChar == '\r') || ((lastChar > 0) && isspace(lastChar)))
		{
			str[strLen - 1] = 0;
			strLen--;
			continue;
		}

		break;
	}
}

static std::wstring GetFullPath(const wchar_t* path)
{
	wchar_t tmpBuf[PATH_BUFFER_SIZE];
	GetFullPathName(path, ARRAY_SIZE(tmpBuf), tmpBuf, NULL);
	return tmpBuf;
}

static int EnumFiles(const std::wstring& baseAbsPath, const std::wstring& pathPrefix, StringList &destList, int64_t &totalSize, bool recursive, FilterCompareProc filterProc, HANDLE filterData)
{
	wstring strBasePath = PrependLongPrefix(baseAbsPath);
	IncludeTrailingPathDelim(strBasePath);
	strBasePath.append(L"*.*");

	WIN32_FIND_DATA fd;
	HANDLE hFind;
	int numFound = 0;

	hFind = FindFirstFile(strBasePath.c_str(), &fd);
	if (hFind == INVALID_HANDLE_VALUE) return 0;

	do 
	{
		if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
			continue;

		if (filterProc && !filterProc(&fd, filterData))
			continue;

		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		{
			// Is a file
			destList.push_back(pathPrefix + fd.cFileName);
			totalSize += (fd.nFileSizeLow + ((int64_t)fd.nFileSizeHigh << 32));
			numFound++;
		}
		else if (recursive)
		{
			// Is a directory
			wstring strNexBasePath = baseAbsPath + fd.cFileName;
			strNexBasePath += L"\\";
			wstring strNextPrefix = pathPrefix + fd.cFileName;
			strNextPrefix += L"\\";

			numFound += EnumFiles(strNexBasePath, strNextPrefix, destList, totalSize, recursive, filterProc, filterData);
		}

	} while (FindNextFile(hFind, &fd));

	FindClose(hFind);
	return numFound;
}

int PrepareFilesList(const wchar_t* basePath, const wchar_t* basePrefix, StringList &destList, int64_t &totalSize, bool recursive, FilterCompareProc filterProc, HANDLE filterData)
{
	wstring strBasePath = GetFullPath(basePath);
	wstring strStartPrefix(basePrefix);

	IncludeTrailingPathDelim(strBasePath);
	IncludeTrailingPathDelim(strStartPrefix);

	return EnumFiles(strBasePath, strStartPrefix, destList, totalSize, recursive, filterProc, filterData);
}

bool CopyTextToClipboard( std::wstring &data )
{
	if (!OpenClipboard(GetConsoleWindow())) return false;
	EmptyClipboard();

	size_t dataLen = (data.size() + 1) * sizeof(wchar_t);
	bool ret = false;
	
	HANDLE hMem = GlobalAlloc(GMEM_MOVEABLE, dataLen);
	if (hMem != NULL)
	{
		wchar_t* pMemPtr = (wchar_t*) GlobalLock(hMem);
		wcscpy_s(pMemPtr, data.size() + 1, data.c_str());
		GlobalUnlock(hMem);

		ret = SetClipboardData(CF_UNICODETEXT, hMem) != NULL;
	}

	CloseClipboard();
	return ret;
}

bool CopyTextToClipboard( std::vector<std::wstring> &data )
{
	std::wstringstream sstr;
	for (size_t i = 0; i < data.size(); i++)
	{
		sstr << data[i] << '\r' << '\n';
	}
	return CopyTextToClipboard(sstr.str());
}

bool GetTextFromClipboard(std::string &data)
{
	if (!OpenClipboard(NULL)) return false;

	bool ret = false;
	HANDLE hData = GetClipboardData(CF_TEXT);
	if (hData != NULL)
	{
		char* pText = (char*) GlobalLock(hData);
		data = std::string(pText);
		GlobalUnlock(hData);
		ret = true;
	}

	CloseClipboard();
	return ret;
}

std::wstring ExtractFileName( const std::wstring & fullPath )
{
	size_t pos = fullPath.find_last_of(L"/\\");
	if (pos == wstring::npos)
		return fullPath;
	else
		return fullPath.substr(pos + 1);
}

std::wstring ExtractFileExt( const std::wstring & path )
{
	std::wstring fileName = ExtractFileName(path);
	size_t pos = fileName.rfind('.');
	return (pos != wstring::npos) ? fileName.substr(pos) : L"";
}

std::wstring FormatString(const std::wstring fmt_str, ...)
{
	const size_t MaxBufferSize = 1024;

	wchar_t formatted[MaxBufferSize];
	va_list ap;

	wcscpy_s(&formatted[0], MaxBufferSize, fmt_str.c_str());
	va_start(ap, fmt_str);
	_vsnwprintf_s(&formatted[0], MaxBufferSize, MaxBufferSize, fmt_str.c_str(), ap);
	va_end(ap);

	return std::wstring(formatted);
}

std::wstring ConvertToUnicode( const std::string &str, int cp )
{
	int numChars = MultiByteToWideChar(cp, 0, str.c_str(), -1, NULL, 0);
	std::unique_ptr<wchar_t[]> tmpBuf(new wchar_t[numChars + 1]);
	MultiByteToWideChar(cp, 0, str.c_str(), -1, tmpBuf.get(), numChars + 1);
	return tmpBuf.get();
}

void TrimStr(std::string &str)
{
	while(str.length() > 0 && isspace(str.back()))
		str.pop_back();
	while(str.length() > 0 && isspace(str.front()))
		str.erase(0, 1);
}

bool SameText(const wchar_t* str1, const wchar_t* str2)
{
	return _wcsicmp(str1, str2) == 0;
}

std::wstring PrependLongPrefix(const std::wstring &basePath)
{
	std::wstring finalPath = basePath;
	
	// If path is network path (starts with \\) then prefix is not needed
	if ((finalPath.size() > 2) && (finalPath[0] != '\\' || finalPath[1] != '\\'))
		finalPath.insert(0, L"\\\\?\\");

	return finalPath;
}
