// IntChecker2.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "far2/plugin.hpp"
#include "Utils.h"
#include "RegistrySettings.h"
#include "hashing.h"

PluginStartupInfo FarSInfo;
static FARSTANDARDFUNCTIONS FSF;

// Plugin settings
static bool optDetectHashFiles = true;
static bool optClearSelectionOnComplete = true;
static bool optConfirmAbort = true;
static int optDefaultAlgo = RHASH_MD5;
static wchar_t optPrefix[32] = L"check";

static rhash_ids s_SupportedAlgos[] = {RHASH_CRC32, RHASH_MD5, RHASH_SHA1, RHASH_SHA256, RHASH_SHA512, RHASH_WHIRLPOOL};

enum HashOutputTargets
{
	OT_SINGLEFILE,
	OT_SEPARATEFILES,
	OT_DISPLAY
};

struct ProgressContext
{
	wstring FileName;
	
	int64_t TotalFileSize;
	int64_t ProcessedBytes;

	int TotalFilesCount;
	int FileIndex;

	int FileProgress;
};

// --------------------------------------- Service functions -------------------------------------------------

static const wchar_t* GetLocMsg(int MsgID)
{
	return FarSInfo.GetMsg(FarSInfo.ModuleNumber, MsgID);
}

static void DisplayMessage(const wchar_t* headerText, const wchar_t* messageText, const wchar_t* errorItem, bool isError, bool isInteractive)
{
	static const wchar_t* MsgLines[3];
	MsgLines[0] = headerText;
	MsgLines[1] = messageText;
	MsgLines[2] = errorItem;

	int linesNum = (errorItem) ? 3 : 2;
	int flags = 0;
	if (isError) flags |= FMSG_WARNING;
	if (isInteractive) flags |= FMSG_MB_OK;

	FarSInfo.Message(FarSInfo.ModuleNumber, flags, NULL, MsgLines, linesNum, 0);
}

static void DisplayMessage(int headerMsgID, int textMsgID, const wchar_t* errorItem, bool isError, bool isInteractive)
{
	DisplayMessage(GetLocMsg(headerMsgID), GetLocMsg(textMsgID), errorItem, isError, isInteractive);
}

// --------------------------------------- Local functions ---------------------------------------------------

static void LoadSettings()
{
	RegistrySettings regOpts(FarSInfo.RootKey);
	if (regOpts.Open())
	{
		//
	}
}

static void SaveSettings()
{
	RegistrySettings regOpts(FarSInfo.RootKey);
	if (regOpts.Open(true))
	{
		//
	}
}

// Returns true if file is recognized as hash list
static bool RunValidateFiles(const wchar_t* hashListPath, bool silent)
{
	//TODO: implement
	return false;
}

static bool AskForHashGenerationParams(rhash_ids &selectedAlgo, bool &clearSelection, bool &recursive, HashOutputTargets &outputTarget, wchar_t* outputFileName)
{
	FarDialogItem DialogItems []={
		/*0*/{DI_DOUBLEBOX,		3, 1, 41,19, 0, 0, 0, 0, L"Generate"},

		/*1*/{DI_TEXT,			5, 2,  0, 0, 0, 0, 0, 0, L"Algorithm", 0},
		/*2*/{DI_RADIOBUTTON,	6, 3,  0, 0, 0, (selectedAlgo==RHASH_CRC32), DIF_GROUP, 0, L"&1. CRC32"},
		/*3*/{DI_RADIOBUTTON,	6, 4,  0, 0, 0, (selectedAlgo==RHASH_MD5), 0, 0, L"&2. MD5"},
		/*4*/{DI_RADIOBUTTON,	6, 5,  0, 0, 0, (selectedAlgo==RHASH_SHA1), 0, 0, L"&3. SHA1"},
		/*5*/{DI_RADIOBUTTON,	6, 6,  0, 0, 0, (selectedAlgo==RHASH_SHA256), 0, 0, L"&4. SHA256"},
		/*6*/{DI_RADIOBUTTON,	6, 7,  0, 0, 0, (selectedAlgo==RHASH_SHA512), 0, 0, L"&5. SHA512"},
		/*7*/{DI_RADIOBUTTON,	6, 8,  0, 0, 0, (selectedAlgo==RHASH_WHIRLPOOL), 0, 0, L"&6. Whirlpool"},
		
		/*8*/{DI_TEXT,			3, 9,  0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L""},
		/*9*/{DI_TEXT,			5,10,  0, 0, 0, 0, 0, 0, L"Output to", 0},
		/*10*/{DI_RADIOBUTTON,	6,11,  0, 0, 0, 1, DIF_GROUP, 0, L"&File"},
		/*11*/{DI_RADIOBUTTON,	6,12,  0, 0, 0, 0, 0, 0, L"&Separate hash files"},
		/*12*/{DI_RADIOBUTTON,	6,13,  0, 0, 0, 0, 0, 0, L"&Display"},
		/*13*/{DI_EDIT,			15,11,38, 0, 1, 0, DIF_EDITEXPAND|DIF_EDITPATH,0, L"hashfile", 0},
		
		/*14*/{DI_TEXT,			3,14,  0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L""},
		/*15*/{DI_CHECKBOX,		5,15,  0, 0, 0, recursive, 0, 0, L"Process directories &recursively"},
		/*16*/{DI_CHECKBOX,		5,16,  0, 0, 0, clearSelection, 0, 0, L"&Clear selection"},
		
		/*17*/{DI_TEXT,			3,17,  0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L"", 0},
		/*18*/{DI_BUTTON,		0,18,  0,13, 0, 0, DIF_CENTERGROUP, 1, L"Run", 0},
		/*19*/{DI_BUTTON,		0,18,  0,13, 0, 0, DIF_CENTERGROUP, 0, L"Cancel", 0},
	};
	size_t numDialogItems = sizeof(DialogItems) / sizeof(DialogItems[0]);

	HANDLE hDlg = FarSInfo.DialogInit(FarSInfo.ModuleNumber, -1, -1, 45, 21, L"GenerateParams", DialogItems, numDialogItems, 0, 0, FarSInfo.DefDlgProc, 0);

	bool retVal = false;
	if (hDlg != INVALID_HANDLE_VALUE)
	{
		int ExitCode = FarSInfo.DialogRun(hDlg);
		if (ExitCode == numDialogItems - 2) // OK was pressed
		{
			retVal = true;
		}
		FarSInfo.DialogFree(hDlg);
	}
	return retVal;
}

static bool CALLBACK FileHashingProgress(HANDLE context, int64_t bytesProcessed)
{
	if (CheckEsc())
		return false;

	if (context == NULL) return true;

	ProgressContext* prCtx = (ProgressContext*) context;
	prCtx->ProcessedBytes += bytesProcessed;

	int nFileProgress = (prCtx->TotalFileSize > 0) ? (int) ((prCtx->ProcessedBytes * 100) / prCtx->TotalFileSize) : 0;

	if (nFileProgress != prCtx->FileProgress)
	{
		prCtx->FileProgress = nFileProgress;

		static wchar_t szFileProgressLine[100] = {0};
		swprintf_s(szFileProgressLine, ARRAY_SIZE(szFileProgressLine), L"File: %d/%d. Progress: %2d%%", prCtx->FileIndex, prCtx->TotalFilesCount, nFileProgress);

		static const wchar_t* InfoLines[4];
		InfoLines[0] = L"Processing";
		InfoLines[1] = L"Generating hash";
		InfoLines[2] = szFileProgressLine;
		InfoLines[3] = prCtx->FileName.c_str();

		FarSInfo.Message(FarSInfo.ModuleNumber, 0, NULL, InfoLines, ARRAY_SIZE(InfoLines), 0);

		// Win7 only feature
		if (prCtx->TotalFileSize > 0)
		{
			PROGRESSVALUE pv;
			pv.Completed = prCtx->ProcessedBytes;
			pv.Total = prCtx->TotalFileSize;
			FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_SETPROGRESSVALUE, &pv);
		}
	}
	
	return true;
}

static void RunGenerateHashes()
{
	// Generation params
	rhash_ids genAlgo = (rhash_ids) optDefaultAlgo;
	bool clearSelectionDone = optClearSelectionOnComplete;
	bool recursive = true;
	HashOutputTargets outputTarget = OT_SINGLEFILE;
	wchar_t outputFile[MAX_PATH] = {0};

	if (!AskForHashGenerationParams(genAlgo, clearSelectionDone, recursive, outputTarget, outputFile))
		return;

	StringList filesToProcess;
	HashList hashes(genAlgo);
	wstring strPanelDir;

	// Prepare files list
	{
		FarScreenSave screen;
		DisplayMessage(L"Processing", L"Preparing file list", NULL, false, false);

		PanelInfo pi = {0};
		if (!FarSInfo.Control(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, (LONG_PTR)&pi)
			|| (pi.SelectedItemsNumber <= 0) || (pi.PanelType != PTYPE_FILEPANEL))
		{
			DisplayMessage(L"Error", L"Can not work with this panel", NULL, true, true);
			return;
		}

		{
			wchar_t szNameBuffer[PATH_BUFFER_SIZE];
			FarSInfo.Control(PANEL_ACTIVE, FCTL_GETPANELDIR, ARRAY_SIZE(szNameBuffer), (LONG_PTR) szNameBuffer);
			IncludeTrailingPathDelim(szNameBuffer, ARRAY_SIZE(szNameBuffer));
			strPanelDir = szNameBuffer;
		}

		for (int i = 0; i < pi.SelectedItemsNumber; i++)
		{
			size_t requiredBytes = FarSInfo.Control(PANEL_ACTIVE, FCTL_GETSELECTEDPANELITEM, i, NULL);
			if (requiredBytes == 0) continue; //TODO: check and debug

			PluginPanelItem *PPI = (PluginPanelItem*)malloc(requiredBytes);
			if (PPI)
			{
				FarSInfo.Control(PANEL_ACTIVE, FCTL_GETSELECTEDPANELITEM, i, (LONG_PTR)PPI);
				if ((PPI->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
				{
					filesToProcess.push_back(PPI->FindData.lpwszFileName);
				}
				else
				{
					wstring strSelectedDir = strPanelDir + PPI->FindData.lpwszFileName;
					PrepareFilesList(strSelectedDir.c_str(), filesToProcess, recursive);
				}
				free(PPI);
			}

			if (clearSelectionDone)
				FarSInfo.Control(PANEL_ACTIVE, FCTL_CLEARSELECTION, i, NULL);
		}
	}

	FarSInfo.Control(PANEL_ACTIVE, FCTL_REDRAWPANEL, 0, NULL);

	// Perform hashing
	char hashValueBuf[150] = {0};
	ProgressContext progressCtx;
	progressCtx.TotalFilesCount = filesToProcess.size();
	progressCtx.FileIndex = -1;

	// Win7 only feature
	FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_SETPROGRESSSTATE, (void*) PS_INDETERMINATE);

	bool continueSave = true;
	for (StringList::const_iterator cit = filesToProcess.begin(); cit != filesToProcess.end(); cit++)
	{
		wstring strNextFile = *cit;
		wstring strFullPath = strPanelDir + strNextFile;

		progressCtx.FileName = strNextFile;
		progressCtx.FileIndex++;
		progressCtx.ProcessedBytes = 0;
		progressCtx.TotalFileSize = GetFileSize_i64(strFullPath.c_str());
		progressCtx.FileProgress = 0;

		{
			FarScreenSave screen;
			int genRetVal = GenerateHash(strFullPath.c_str(), genAlgo, hashValueBuf, FileHashingProgress, &progressCtx);

			if (genRetVal == GENERATE_ABORTED)
			{
				// Exit silently
				continueSave = false;
				break;
			}
			else if (genRetVal == GENERATE_ERROR)
			{
				DisplayMessage(L"Error", L"Error during hash generation", strNextFile.c_str(), true, true);
				continueSave = false;
				break;
			}
		}

		hashes.SetFileHash(strNextFile.c_str(), hashValueBuf);
	}

	FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_SETPROGRESSSTATE, (void*) PS_NOPROGRESS);
	FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_PROGRESSNOTIFY, 0);

	if (!continueSave) return;

	// Display/save hash list
}

static void RunComparePanels()
{
	//TODO: implement
	DisplayMessage(L"Not implemented", L"Panels Compare", NULL, false, true);
}

// ------------------------------------- Exported functions --------------------------------------------------

int WINAPI GetMinFarVersionW(void)
{
	return FARMANAGERVERSION;
}

void WINAPI SetStartupInfoW(const struct PluginStartupInfo *Info)
{
	FarSInfo = *Info;
	FSF = *Info->FSF;
	FarSInfo.FSF = &FSF;

	LoadSettings();
}

void WINAPI ExitFARW( void )
{
	//
}

void WINAPI GetPluginInfoW(struct PluginInfo *Info)
{
	Info->StructSize = sizeof(PluginInfo);
	Info->Flags = 0;

	static wchar_t *PluginMenuStrings[1];
	PluginMenuStrings[0] = L"Integrity Checker";
	static wchar_t *PluginConfigStrings[1];
	PluginConfigStrings[0] = L"Integrity Checker";

	Info->PluginMenuStrings = PluginMenuStrings;
	Info->PluginMenuStringsNumber = sizeof(PluginMenuStrings) / sizeof(PluginMenuStrings[0]);
	Info->PluginConfigStrings = PluginConfigStrings;
	Info->PluginConfigStringsNumber = sizeof(PluginConfigStrings) / sizeof(PluginConfigStrings[0]);
	Info->CommandPrefix = optPrefix;
}

int WINAPI ConfigureW(int ItemNumber)
{
	//TODO: implement
	return FALSE;
}

HANDLE WINAPI OpenPluginW(int OpenFrom, INT_PTR Item)
{
	if (OpenFrom == OPEN_COMMANDLINE)
	{
		// We are from prefix
		if (!RunValidateFiles((wchar_t*) Item, true))
			DisplayMessage(L"Error", L"File is not a valid hash list", NULL, true, true);
	}
	else if (OpenFrom == OPEN_PLUGINSMENU)
	{
		// We are from regular plug-ins menu

		FarMenuItem MenuItems[] = {
			{L"&Generate Hashes", 1, 0, 0},
			{L"Compare &Panels", 0, 0, 0}
		};

		int nMItem = FarSInfo.Menu(FarSInfo.ModuleNumber, -1, -1, 0, 0, L"Integrity Checker", NULL, NULL, NULL, NULL, MenuItems, ARRAY_SIZE(MenuItems));

		switch (nMItem)
		{
			case 0:
				RunGenerateHashes();
				break;
			case 1:
				RunComparePanels();
				break;
		}
	} // OpenFrom check
		
	return INVALID_HANDLE_VALUE;
}

HANDLE WINAPI OpenFilePluginW(const wchar_t *Name, const unsigned char *Data, int DataSize, int OpMode)
{
	RunValidateFiles(Name, true);
	return INVALID_HANDLE_VALUE;
}
