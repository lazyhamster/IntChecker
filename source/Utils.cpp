#include "StdAfx.h"
#include "far2/plugin.hpp"
#include "Utils.h"

extern PluginStartupInfo FarSInfo;

///////////////////////////////////////////////////////////////////////////////
//							 FarScreenSave									 //
///////////////////////////////////////////////////////////////////////////////

FarScreenSave::FarScreenSave()
{
	hScreen = FarSInfo.SaveScreen(0, 0, -1, -1);
}

FarScreenSave::~FarScreenSave()
{
	FarSInfo.RestoreScreen(hScreen);
}

///////////////////////////////////////////////////////////////////////////////
//							 Various routines								 //
///////////////////////////////////////////////////////////////////////////////

bool ArePanelsComparable()
{
	PanelInfo PnlAct, PnlPas;
	if (!FarSInfo.Control(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, (LONG_PTR) &PnlAct)
		|| !FarSInfo.Control(PANEL_PASSIVE, FCTL_GETPANELINFO, 0, (LONG_PTR) &PnlPas))
		return false;

	if (PnlAct.PanelType != PTYPE_FILEPANEL || PnlAct.PanelType != PnlPas.PanelType || PnlAct.Plugin || PnlPas.Plugin)
		return false;

	wchar_t *wszActivePanelDir, *wszPassivePanelDir;
	int nBufSize;

	nBufSize = FarSInfo.Control(PANEL_ACTIVE, FCTL_GETPANELDIR, 0, NULL);
	wszActivePanelDir = (wchar_t*) malloc((nBufSize+1) * sizeof(wchar_t));
	FarSInfo.Control(PANEL_ACTIVE, FCTL_GETPANELDIR, 0, (LONG_PTR) wszActivePanelDir);

	nBufSize = FarSInfo.Control(PANEL_PASSIVE, FCTL_GETPANELDIR, 0, NULL);
	wszPassivePanelDir = (wchar_t*) malloc((nBufSize+1) * sizeof(wchar_t));
	FarSInfo.Control(PANEL_PASSIVE, FCTL_GETPANELDIR, 0, (LONG_PTR) wszPassivePanelDir);

	bool fSameDir = wcscmp(wszActivePanelDir, wszPassivePanelDir) != 0;

	free(wszActivePanelDir);
	free(wszPassivePanelDir);

	return !fSameDir;
}

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

bool IsAbsPath(const wchar_t* path)
{
	return (wcslen(path) > 3) && ((path[1] == L':' && path[2] == L'\\') || (path[0] == L'\\' && path[1] == L'\\'));
}

void IncludeTrailingPathDelim(wchar_t *pathBuf, size_t bufMaxSize)
{
	size_t nPathLen = wcslen(pathBuf);
	if (pathBuf[nPathLen - 1] != '\\')
		wcscat_s(pathBuf, bufMaxSize, L"\\");
}

void IncludeTrailingPathDelim(wstring &pathStr)
{
	size_t nStrLen = pathStr.length();
	if (pathStr[nStrLen - 1] != '\\')
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

bool IsFile( const wchar_t* path )
{
	WIN32_FIND_DATA fd = {0};
	HANDLE hFind = FindFirstFile(path, &fd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		FindClose(hFind);
		return (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
	}

	return false;
}

void TrimRight( char* str )
{
	size_t strLen = strlen(str);
	while (strLen > 0)
	{
		char lastChar = str[strLen - 1];
		if (isspace(lastChar) || (lastChar == '\n') || (lastChar == '\r'))
		{
			str[strLen - 1] = 0;
			strLen--;
			continue;
		}

		break;
	}
}

static wstring GetFullPath(const wchar_t* path)
{
	wchar_t tmpBuf[4096];
	GetFullPathName(path, ARRAY_SIZE(tmpBuf), tmpBuf, NULL);
	return tmpBuf;
}

static int EnumFiles(const wstring& baseAbsPath, const wstring& pathPrefix, StringList &destList, int64_t &totalSize, bool recursive)
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

		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			if (recursive)
			{
				wstring strNexBasePath = baseAbsPath + fd.cFileName;
				strNexBasePath += L"\\";
				wstring strNextPrefix = pathPrefix + fd.cFileName;
				strNextPrefix += L"\\";

				numFound += EnumFiles(strNexBasePath, strNextPrefix, destList, totalSize, recursive);
			}
		}
		else
		{
			destList.push_back(pathPrefix + fd.cFileName);
			totalSize += (fd.nFileSizeLow + ((int64_t)fd.nFileSizeHigh << 32));
			numFound++;
		}
	} while (FindNextFile(hFind, &fd));

	FindClose(hFind);
	return numFound;
}

int PrepareFilesList(const wchar_t* basePath, const wchar_t* basePrefix, StringList &destList, int64_t &totalSize, bool recursive)
{
	wstring strBasePath = GetFullPath(basePath);
	wstring strStartPrefix(basePrefix);

	IncludeTrailingPathDelim(strBasePath);
	IncludeTrailingPathDelim(strStartPrefix);

	return EnumFiles(strBasePath, strStartPrefix, destList, totalSize, recursive);
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
