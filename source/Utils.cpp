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
		return ((int64_t)fd.nFileSizeHigh >> 32) + fd.nFileSizeLow;
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
