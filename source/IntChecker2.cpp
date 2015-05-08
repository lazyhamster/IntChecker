// IntChecker2.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "far2/plugin.hpp"
#include "Utils.h"
#include "RegistrySettings.h"

#include <boost/bind.hpp>

#include "FarCommon.h"
#include "farhelpers/Far2Menu.hpp"

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

static bool ConfirmMessage(const wchar_t* headerText, const wchar_t* messageText, bool isWarning)
{
	static const wchar_t* MsgLines[2];
	MsgLines[0] = headerText;
	MsgLines[1] = messageText;

	int flags = FMSG_MB_YESNO;
	if (isWarning) flags |= FMSG_WARNING;

	int resp = FarSInfo.Message(FarSInfo.ModuleNumber, flags, NULL, MsgLines, 2, 0);
	return (resp == 0);
}

static bool ConfirmMessage(int headerMsgID, int textMsgID, bool isWarning)
{
	return ConfirmMessage(GetLocMsg(headerMsgID), GetLocMsg(textMsgID), isWarning);
}

static bool DlgHlp_GetSelectionState(HANDLE hDlg, int ctrlIndex)
{
	FarDialogItem *dlgItem;
	int retVal;

	dlgItem = (FarDialogItem*) malloc(FarSInfo.SendDlgMessage(hDlg, DM_GETDLGITEM, ctrlIndex, NULL));
	FarSInfo.SendDlgMessage(hDlg, DM_GETDLGITEM, ctrlIndex, (LONG_PTR) dlgItem);
	retVal = dlgItem->Selected;
	free(dlgItem);

	return retVal != 0;
}

static void DlgHlp_GetEditBoxText(HANDLE hDlg, int ctrlIndex, wstring &buf)
{
	FarDialogItem *dlgItem;

	dlgItem = (FarDialogItem*) malloc(FarSInfo.SendDlgMessage(hDlg, DM_GETDLGITEM, ctrlIndex, NULL));
	FarSInfo.SendDlgMessage(hDlg, DM_GETDLGITEM, ctrlIndex, (LONG_PTR) dlgItem);

	buf = dlgItem->PtrData;

	free(dlgItem);
}

static bool DlgHlp_GetEditBoxText(HANDLE hDlg, int ctrlIndex, wchar_t* buf, size_t bufSize)
{
	wstring tmpStr;
	DlgHlp_GetEditBoxText(hDlg, ctrlIndex, tmpStr);

	if (tmpStr.size() < bufSize)
	{
		wcscpy_s(buf, bufSize, tmpStr.c_str());
		return true;
	}

	return false;
}

static bool GetPanelDir(HANDLE hPanel, wstring& dirStr)
{
	wchar_t *wszPanelDir;
	int nBufSize;
	bool ret = false;

	nBufSize = FarSInfo.Control(hPanel, FCTL_GETPANELDIR, 0, NULL);
	wszPanelDir = (wchar_t*) malloc((nBufSize+1) * sizeof(wchar_t));
	if (FarSInfo.Control(hPanel, FCTL_GETPANELDIR, nBufSize + 1, (LONG_PTR) wszPanelDir))
	{
		dirStr.assign(wszPanelDir);
		IncludeTrailingPathDelim(dirStr);
		ret = true;
	}

	free(wszPanelDir);
	return ret;
}

static bool GetSelectedPanelItemPath(wstring& nameStr)
{
	nameStr.clear();

	PanelInfo pi = {0};
	if (FarSInfo.Control(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, (LONG_PTR)&pi))
		if ((pi.SelectedItemsNumber == 1) && (pi.PanelType == PTYPE_FILEPANEL))
		{
			wstring panelDir;
			GetPanelDir(PANEL_ACTIVE, panelDir);

			PluginPanelItem *PPI = (PluginPanelItem*)malloc(FarSInfo.Control(PANEL_ACTIVE, FCTL_GETCURRENTPANELITEM, 0, NULL));
			if (PPI)
			{
				FarSInfo.Control(PANEL_ACTIVE, FCTL_GETCURRENTPANELITEM, 0, (LONG_PTR)PPI);
				nameStr = panelDir + PPI->FindData.lpwszFileName;
				free(PPI);
			}
		}

		return (nameStr.size() > 0);
}

static void GetSelectedPanelFiles(PanelInfo &pi, wstring &panelDir, StringList &vDest, int64_t &totalSize, bool recursive)
{
	for (int i = 0; i < pi.SelectedItemsNumber; i++)
	{
		size_t requiredBytes = FarSInfo.Control(PANEL_ACTIVE, FCTL_GETSELECTEDPANELITEM, i, NULL);
		PluginPanelItem *PPI = (PluginPanelItem*)malloc(requiredBytes);
		if (PPI)
		{
			FarSInfo.Control(PANEL_ACTIVE, FCTL_GETSELECTEDPANELITEM, i, (LONG_PTR)PPI);
			if (wcscmp(PPI->FindData.lpwszFileName, L"..") && wcscmp(PPI->FindData.lpwszFileName, L"."))
			{
				if ((PPI->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
				{
					vDest.push_back(PPI->FindData.lpwszFileName);
					totalSize += PPI->FindData.nFileSize;
				}
				else
				{
					wstring strSelectedDir = panelDir + PPI->FindData.lpwszFileName;
					PrepareFilesList(strSelectedDir.c_str(), PPI->FindData.lpwszFileName, vDest, totalSize, recursive);
				}
			}
			free(PPI);
		}
	}
}

// --------------------------------------- Local functions ---------------------------------------------------

static void LoadSettings()
{
	RegistrySettings regOpts(FarSInfo.RootKey);
	if (regOpts.Open())
	{
		regOpts.GetValue(L"DetectHashFiles", optDetectHashFiles);
		regOpts.GetValue(L"ClearSelection", optClearSelectionOnComplete);
		regOpts.GetValue(L"ConfirmAbort", optConfirmAbort);
		regOpts.GetValue(L"DefaultHash", optDefaultAlgo);
		regOpts.GetValue(L"Prefix", optPrefix, ARRAY_SIZE(optPrefix));
		regOpts.GetValue(L"UsePrefix", optUsePrefix);
		regOpts.GetValue(L"AutoExtension", optAutoExtension);
		regOpts.GetValue(L"HashInUppercase", optHashUppercase);
	}
}

static void SaveSettings()
{
	RegistrySettings regOpts(FarSInfo.RootKey);
	if (regOpts.Open(true))
	{
		regOpts.SetValue(L"DetectHashFiles", optDetectHashFiles);
		regOpts.SetValue(L"ClearSelection", optClearSelectionOnComplete);
		regOpts.SetValue(L"ConfirmAbort", optConfirmAbort);
		regOpts.SetValue(L"DefaultHash", optDefaultAlgo);
		regOpts.SetValue(L"Prefix", optPrefix);
		regOpts.SetValue(L"UsePrefix", optUsePrefix);
		regOpts.SetValue(L"AutoExtension", optAutoExtension);
		regOpts.SetValue(L"HashInUppercase", optHashUppercase);
	}
}

static bool CALLBACK FileHashingProgress(HANDLE context, int64_t bytesProcessed)
{
	if (CheckEsc())
	{
		if (optConfirmAbort && ConfirmMessage(GetLocMsg(MSG_DLG_CONFIRM), GetLocMsg(MSG_DLG_ASK_ABORT), true))
			return false;
	}

	if (context == NULL) return true;

	ProgressContext* prCtx = (ProgressContext*) context;
	prCtx->CurrentFileProcessedBytes += bytesProcessed;
	prCtx->TotalProcessedBytes += bytesProcessed;

	int nFileProgress = (prCtx->CurrentFileSize > 0) ? (int) ((prCtx->CurrentFileProcessedBytes * 100) / prCtx->CurrentFileSize) : 0;
	int nTotalProgress = (prCtx->TotalFilesSize > 0) ? (int) ((prCtx->TotalProcessedBytes * 100) / prCtx->TotalFilesSize) : 0;

	if (nFileProgress != prCtx->FileProgress || nTotalProgress != prCtx->TotalProgress)
	{
		prCtx->FileProgress = nFileProgress;
		prCtx->TotalProgress = nTotalProgress;

		static wchar_t szFileProgressLine[100] = {0};
		swprintf_s(szFileProgressLine, ARRAY_SIZE(szFileProgressLine), L"File: %d / %d. Progress: %2d%% / %2d%%", prCtx->CurrentFileIndex + 1, prCtx->TotalFilesCount, nFileProgress, nTotalProgress);

		static const wchar_t* InfoLines[4];
		InfoLines[0] = GetLocMsg(MSG_DLG_PROCESSING);
		InfoLines[1] = GetLocMsg(MSG_DLG_GENERATING);
		InfoLines[2] = szFileProgressLine;
		InfoLines[3] = prCtx->FileName.c_str();

		FarSInfo.Message(FarSInfo.ModuleNumber, 0, NULL, InfoLines, ARRAY_SIZE(InfoLines), 0);

		// Win7 only feature
		if (prCtx->TotalFilesSize > 0)
		{
			PROGRESSVALUE pv;
			pv.Completed = prCtx->TotalProcessedBytes;
			pv.Total = prCtx->TotalFilesSize;
			FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_SETPROGRESSVALUE, &pv);
		}
	}

	return true;
}

static void SelectFilesOnPanel(HANDLE hPanel, vector<wstring> &fileNames, bool isSelected)
{
	if (fileNames.size() == 0) return;
	
	PanelInfo pi = {0};
	FarSInfo.Control(hPanel, FCTL_GETPANELINFO, 0, (LONG_PTR)&pi);

	FarSInfo.Control(hPanel, FCTL_BEGINSELECTION, 0, NULL);
	for (int i = 0; i < pi.ItemsNumber; i++)
	{
		PluginPanelItem *PPI = (PluginPanelItem*) malloc(FarSInfo.Control(hPanel, FCTL_GETPANELITEM, i, NULL));
		if (PPI)
		{
			FarSInfo.Control(hPanel, FCTL_GETPANELITEM, i, (LONG_PTR)PPI);
			if (std::find(fileNames.begin(), fileNames.end(), PPI->FindData.lpwszFileName) != fileNames.end())
			{
				FarSInfo.Control(hPanel, FCTL_SETSELECTION, i, isSelected ? TRUE : FALSE);
			}
			free(PPI);
		}
	}
	FarSInfo.Control(hPanel, FCTL_ENDSELECTION, 0, NULL);
	FarSInfo.Control(hPanel, FCTL_REDRAWPANEL, 0, NULL);
}

static void DisplayValidationResults(std::vector<std::wstring> &vMismatchList, std::vector<std::wstring> &vMissingList, int numSkipped)
{
	if (vMismatchList.size() == 0 && vMissingList.size() == 0)
	{
		// If everything is fine then just display simple message
		static wchar_t wszGoodMessage[256];
		if (numSkipped == 0)
			wcscpy_s(wszGoodMessage, ARRAY_SIZE(wszGoodMessage), GetLocMsg(MSG_DLG_NO_MISMATCHES));
		else
			swprintf_s(wszGoodMessage, ARRAY_SIZE(wszGoodMessage), L"No mismatches found (%d file(s) were skipped)", numSkipped);

		DisplayMessage(GetLocMsg(MSG_DLG_VALIDATION_COMPLETE), wszGoodMessage, NULL, false, true);
	}
	else
	{
		// Otherwise display proper list of invalid/missing files
		
		size_t nNumListItems = (vMismatchList.size() > 0 ? vMismatchList.size() + 1 : 0)
			+ (vMissingList.size() > 0 ? vMissingList.size() + 1 : 0);
		FarListItem* mmListItems = (FarListItem*) malloc(nNumListItems * sizeof(FarListItem));
		FarList mmList = {(int)nNumListItems, mmListItems};
		memset(mmListItems, 0, nNumListItems * sizeof(FarListItem));

		int nDlgWidth = 68;
		int nDlgHeight = 21;

		FarDialogItem DialogItems []={
			/*00*/ {DI_DOUBLEBOX, 3, 1,nDlgWidth-4,nDlgHeight-2, 0, 0, 0,0, GetLocMsg(MSG_DLG_VALIDATION_COMPLETE), 0},
			/*01*/ {DI_LISTBOX,   5, 2,nDlgWidth-6,nDlgHeight-5, 0, (DWORD_PTR) &mmList, DIF_LISTNOCLOSE | DIF_LISTNOBOX, 0, NULL, 0},
			/*02*/ {DI_TEXT,	  3,nDlgHeight-4, 0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L"", 0},
			/*03*/ {DI_BUTTON,	  0,nDlgHeight-3, 0, 0, 1, 0, DIF_CENTERGROUP, 1, GetLocMsg(MSG_BTN_CLOSE), 0},
		};

		HANDLE hDlg = FarSInfo.DialogInit(FarSInfo.ModuleNumber, -1, -1, nDlgWidth, nDlgHeight, NULL,
			DialogItems, sizeof(DialogItems) / sizeof(DialogItems[0]), 0, 0, FarSInfo.DefDlgProc, 0);

		size_t nListIndex = 0;
		wchar_t wszMismatchHeader[64] = {0};
		wchar_t wszMissingHeader[64] = {0};

		if (vMismatchList.size() > 0)
		{
			swprintf_s(wszMismatchHeader, GetLocMsg(MSG_DLG_MISMATCHED_FILES), vMismatchList.size());
			
			mmListItems[nListIndex++].Text = wszMismatchHeader;
			for (size_t i = 0; i < vMismatchList.size(); i++)
			{
				wstring &nextFile = vMismatchList[i];
				nextFile.insert(0, L"\t\t");
				mmListItems[nListIndex++].Text = nextFile.c_str();
			}
		}
		if (vMissingList.size() > 0)
		{
			swprintf_s(wszMissingHeader, GetLocMsg(MSG_DLG_MISSING_FILES), vMissingList.size());
			
			mmListItems[nListIndex++].Text = wszMissingHeader;
			for (size_t i = 0; i < vMissingList.size(); i++)
			{
				wstring &nextFile = vMissingList[i];
				nextFile.insert(0, L"\t\t");
				mmListItems[nListIndex++].Text = nextFile.c_str();
			}
		}
		
		FarSInfo.DialogRun(hDlg);
		FarSInfo.DialogFree(hDlg);

		// Select mismatched files that are in the same folder

		vector<wstring> vSameFolderFiles;
		for (size_t i = 0; i < vMismatchList.size(); i++)
		{
			wstring &nextFile = vMismatchList[i];
			if (nextFile.find_first_of(L"\\/") == wstring::npos)
				vSameFolderFiles.push_back(nextFile);
		}

		SelectFilesOnPanel(PANEL_ACTIVE, vSameFolderFiles, true);

		free(mmListItems);
	}
}

// Returns true if file is recognized as hash list
static bool RunValidateFiles(const wchar_t* hashListPath, bool silent)
{
	HashList hashes;
	if (!hashes.LoadList(hashListPath) || (hashes.GetCount() == 0))
	{
		if (!silent)
			DisplayMessage(GetLocMsg(MSG_DLG_ERROR), GetLocMsg(MSG_DLG_NOTVALIDLIST), NULL, true, true);
		return false;
	}

	wstring workDir;
	int nFilesSkipped = 0;
	vector<wstring> vMismatches, vMissing;
	vector<size_t> existingFiles;
	int64_t totalFilesSize = 0;
	char hashValueBuf[150];

	if (!GetPanelDir(PANEL_ACTIVE, workDir))
		return false;

	// Win7 only feature
	FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_SETPROGRESSSTATE, (void*) PS_INDETERMINATE);

	// Prepare files list
	{
		FarScreenSave screen;
		DisplayMessage(GetLocMsg(MSG_DLG_PROCESSING), GetLocMsg(MSG_DLG_PREPARE_LIST), NULL, false, false);

		for (size_t i = 0; i < hashes.GetCount(); i++)
		{
			FileHashInfo fileInfo = hashes.GetFileInfo(i);

			wstring strFullFilePath = MakeAbsPath(fileInfo.Filename, workDir);
			if (IsFile(strFullFilePath.c_str()))
			{
				existingFiles.push_back(i);
				totalFilesSize += GetFileSize_i64(strFullFilePath.c_str());
			}
			else
			{
				vMissing.push_back(fileInfo.Filename);
			}
		}
	}

	if (existingFiles.size() > 0)
	{
		ProgressContext progressCtx;
		progressCtx.TotalFilesCount = (int) existingFiles.size();
		progressCtx.TotalFilesSize = totalFilesSize;
		progressCtx.CurrentFileIndex = -1;

		for (size_t i = 0; i < existingFiles.size(); i++)
		{
			FileHashInfo fileInfo = hashes.GetFileInfo(existingFiles[i]);
			wstring strFullFilePath = MakeAbsPath(fileInfo.Filename, workDir);

			progressCtx.FileName = fileInfo.Filename;
			progressCtx.CurrentFileIndex++;
			progressCtx.CurrentFileProcessedBytes = 0;
			progressCtx.CurrentFileSize = GetFileSize_i64(strFullFilePath.c_str());
			progressCtx.FileProgress = 0;

			{
				FarScreenSave screen;
				int genRetVal = GenerateHash(strFullFilePath.c_str(), fileInfo.GetAlgo(), hashValueBuf, false, FileHashingProgress, &progressCtx);

				if (genRetVal == GENERATE_ABORTED)
				{
					// Exit silently
					break;
				}
				else if (genRetVal == GENERATE_ERROR)
				{
					//TODO: offer retry
					DisplayMessage(L"Error", L"Error during hash generation", fileInfo.Filename.c_str(), true, true);
					break;
				}

				if (_stricmp(fileInfo.HashStr.c_str(), hashValueBuf) != 0)
					vMismatches.push_back(fileInfo.Filename);
			}
		}

		DisplayValidationResults(vMismatches, vMissing, nFilesSkipped);
	}
	else
	{
		DisplayMessage(GetLocMsg(MSG_DLG_NOFILES_TITLE), GetLocMsg(MSG_DLG_NOFILES_TEXT), NULL, true, true);
	}

	FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_SETPROGRESSSTATE, (void*) PS_NOPROGRESS);
	FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_PROGRESSNOTIFY, 0);
	
	return true;
}

static LONG_PTR WINAPI HashParamsDlgProc(HANDLE hDlg, int Msg, int Param1, LONG_PTR Param2)
{
	if (Msg == DN_BTNCLICK && optAutoExtension)
	{
		if (Param2 && (Param1 >= 2) && (Param1 <= 2 + NUMBER_OF_SUPPORTED_HASHES))
		{
			int selectedHashIndex = Param1 - 2;
			wchar_t wszHashFileName[MAX_PATH];

			DlgHlp_GetEditBoxText(hDlg, 13, wszHashFileName, ARRAY_SIZE(wszHashFileName));
			
			// We should only replace extensions if it exists and is one of auto-extensions
			// this way custom names will not be touched when user switch algorithms
			wchar_t* extPtr = wcsrchr(wszHashFileName, '.');
			if (extPtr && *extPtr)
			{
				for (int i = 0; i < NUMBER_OF_SUPPORTED_HASHES; i++)
				{
					if ((i != selectedHashIndex) && (SupportedHashes[i].DefaultExt == extPtr))
					{
						wcscpy_s(extPtr, MAX_PATH - (extPtr - wszHashFileName), SupportedHashes[selectedHashIndex].DefaultExt.c_str());
						FarSInfo.SendDlgMessage(hDlg, DM_SETTEXTPTR, 13, (LONG_PTR) wszHashFileName);
						break;
					}
				}
			}

			return TRUE;
		}
	}

	return FarSInfo.DefDlgProc(hDlg, Msg, Param1, Param2);
}

static bool AskForHashGenerationParams(rhash_ids &selectedAlgo, bool &recursive, HashOutputTargets &outputTarget, wstring &outputFileName, int &storeAbsPaths)
{
	FarDialogItem DialogItems []={
		/*0*/{DI_DOUBLEBOX,		3, 1, 41,20, 0, 0, 0, 0, GetLocMsg(MSG_GEN_TITLE)},

		/*1*/{DI_TEXT,			5, 2, 0, 0, 0, 0, 0, 0, GetLocMsg(MSG_GEN_ALGO), 0},
		/*2*/{DI_RADIOBUTTON,	6, 3, 0, 0, 0, (selectedAlgo==RHASH_CRC32), DIF_GROUP, 0, GetLocMsg(MSG_ALGO_CRC)},
		/*3*/{DI_RADIOBUTTON,	6, 4, 0, 0, 0, (selectedAlgo==RHASH_MD5), 0, 0, GetLocMsg(MSG_ALGO_MD5)},
		/*4*/{DI_RADIOBUTTON,	6, 5, 0, 0, 0, (selectedAlgo==RHASH_SHA1), 0, 0, GetLocMsg(MSG_ALGO_SHA1)},
		/*5*/{DI_RADIOBUTTON,	6, 6, 0, 0, 0, (selectedAlgo==RHASH_SHA256), 0, 0, GetLocMsg(MSG_ALGO_SHA256)},
		/*6*/{DI_RADIOBUTTON,	6, 7, 0, 0, 0, (selectedAlgo==RHASH_SHA512), 0, 0, GetLocMsg(MSG_ALGO_SHA512)},
		/*7*/{DI_RADIOBUTTON,	6, 8, 0, 0, 0, (selectedAlgo==RHASH_WHIRLPOOL), 0, 0, GetLocMsg(MSG_ALGO_WHIRLPOOL)},
		
		/*8*/{DI_TEXT,			3, 9, 0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L""},
		/*9*/{DI_TEXT,			5,10, 0, 0, 0, 0, 0, 0, GetLocMsg(MSG_GEN_TARGET), 0},
		/*10*/{DI_RADIOBUTTON,	6,11, 0, 0, 0, 1, DIF_GROUP, 0, GetLocMsg(MSG_GEN_TO_FILE)},
		/*11*/{DI_RADIOBUTTON,	6,13, 0, 0, 0, 0, 0, 0, GetLocMsg(MSG_GEN_TO_SEPARATE)},
		/*12*/{DI_RADIOBUTTON,	6,14, 0, 0, 0, 0, 0, 0, GetLocMsg(MSG_GEN_TO_SCREEN)},
		/*13*/{DI_EDIT,		   10,12,38, 0, 1, 0, DIF_EDITEXPAND|DIF_EDITPATH,0, outputFileName.c_str(), 0},
		
		/*14*/{DI_TEXT,			3,15, 0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L""},
		/*15*/{DI_CHECKBOX,		5,16, 0, 0, 0, recursive, 0, 0, GetLocMsg(MSG_GEN_RECURSE)},
		/*16*/{DI_CHECKBOX,		5,17, 0, 0, 0, storeAbsPaths, 0, 0, GetLocMsg(MSG_GEN_ABSPATH)},
		
		/*17*/{DI_TEXT,			3,18, 0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L"", 0},
		/*18*/{DI_BUTTON,		0,19, 0,13, 0, 0, DIF_CENTERGROUP, 1, GetLocMsg(MSG_BTN_RUN), 0},
		/*19*/{DI_BUTTON,		0,19, 0,13, 0, 0, DIF_CENTERGROUP, 0, GetLocMsg(MSG_BTN_CANCEL), 0},
	};
	size_t numDialogItems = sizeof(DialogItems) / sizeof(DialogItems[0]);

	HANDLE hDlg = FarSInfo.DialogInit(FarSInfo.ModuleNumber, -1, -1, 45, 22, L"GenerateParams", DialogItems, (unsigned) numDialogItems, 0, 0, HashParamsDlgProc, 0);

	bool retVal = false;
	if (hDlg != INVALID_HANDLE_VALUE)
	{
		int ExitCode = FarSInfo.DialogRun(hDlg);
		if (ExitCode == numDialogItems - 2) // OK was pressed
		{
			recursive = DlgHlp_GetSelectionState(hDlg, 15) != 0;
			storeAbsPaths = DlgHlp_GetSelectionState(hDlg, 16);
			DlgHlp_GetEditBoxText(hDlg, 13, outputFileName);

			for (int i = 0; i < NUMBER_OF_SUPPORTED_HASHES; i++)
			{
				// Selection radios start from index = 2
				if (DlgHlp_GetSelectionState(hDlg, 2 + i))
					selectedAlgo = SupportedHashes[i].AlgoId;
			}

			if (DlgHlp_GetSelectionState(hDlg, 10)) outputTarget = OT_SINGLEFILE;
			else if (DlgHlp_GetSelectionState(hDlg, 11)) outputTarget = OT_SEPARATEFILES;
			else if (DlgHlp_GetSelectionState(hDlg, 12)) outputTarget = OT_DISPLAY;
			
			retVal = true;
		}
		FarSInfo.DialogFree(hDlg);
	}
	return retVal;
}

static void DisplayHashListOnScreen(HashList &list)
{
	int numListItems = (int) list.GetCount();
	FarListItem* hashListItems = new FarListItem[numListItems];
	FarList hashDump = {numListItems, hashListItems};

	int nDlgWidth = 68;
	int nDlgHeight = 21;

	FarDialogItem DialogItems []={
		/*00*/ {DI_DOUBLEBOX, 3, 1,nDlgWidth-4,nDlgHeight-2, 0, 0, 0,0, GetLocMsg(MSG_DLG_CALC_COMPLETE), 0},
		/*01*/ {DI_LISTBOX,   5, 2,nDlgWidth-6,nDlgHeight-5, 0, (DWORD_PTR)&hashDump, DIF_LISTNOCLOSE | DIF_LISTNOBOX, 0, NULL, 0},
		/*02*/ {DI_TEXT,	  3,nDlgHeight-4, 0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L"", 0},
		/*03*/ {DI_BUTTON,	  0,nDlgHeight-3, 0, 0, 1, 0, DIF_CENTERGROUP, 1, GetLocMsg(MSG_BTN_CLOSE), 0},
		/*04*/ {DI_BUTTON,    0,nDlgHeight-3, 0, 0, 0, 0, DIF_CENTERGROUP, 0, GetLocMsg(MSG_BTN_CLIPBOARD), 0},
	};

	vector<wstring> listStrDump;
	for (size_t i = 0; i < list.GetCount(); i++)
	{
		listStrDump.push_back(list.FileInfoToString(i));

		wstring &line = listStrDump[i];
		memset(&hashListItems[i], 0, sizeof(FarListItem));
		hashListItems[i].Text = line.c_str();
	}

	HANDLE hDlg = FarSInfo.DialogInit(FarSInfo.ModuleNumber, -1, -1, nDlgWidth, nDlgHeight, NULL,
		DialogItems, sizeof(DialogItems) / sizeof(DialogItems[0]), 0, 0, FarSInfo.DefDlgProc, 0);

	if (hDlg != INVALID_HANDLE_VALUE)
	{
		int ExitCode = FarSInfo.DialogRun(hDlg);
		if (ExitCode == 4) // clipboard
		{
			CopyTextToClipboard(listStrDump);
		}
		FarSInfo.DialogFree(hDlg);
	}

	delete [] hashListItems;
}

static int DisplayHashGenerateError(const wstring& fileName)
{
	static const wchar_t* DlgLines[7];
	DlgLines[0] = GetLocMsg(MSG_DLG_ERROR);
	DlgLines[1] = GetLocMsg(MSG_DLG_FILE_ERROR);
	DlgLines[2] = fileName.c_str();
	DlgLines[3] = GetLocMsg(MSG_BTN_SKIP);
	DlgLines[4] = GetLocMsg(MSG_BTN_SKIPALL);
	DlgLines[5] = GetLocMsg(MSG_BTN_RETRY);
	DlgLines[6] = GetLocMsg(MSG_BTN_CANCEL);

	return FarSInfo.Message(FarSInfo.ModuleNumber, FMSG_WARNING, NULL, DlgLines, ARRAY_SIZE(DlgLines), 4);
}

static void RunGenerateHashes()
{
	// Check panel for compatibility
	PanelInfo pi = {0};
	if (!FarSInfo.Control(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, (LONG_PTR)&pi) || (pi.SelectedItemsNumber <= 0))
	{
		DisplayMessage(GetLocMsg(MSG_DLG_ERROR), GetLocMsg(MSG_DLG_NO_FILES_SELECTED), NULL, true, true);
		return;
	}
	
	// Generation params
	rhash_ids genAlgo = (rhash_ids) optDefaultAlgo;
	bool recursive = true;
	HashOutputTargets outputTarget = OT_SINGLEFILE;
	wstring outputFile(L"hashlist");
	int storeAbsPaths = 0;

	HashAlgoInfo *selectedHashInfo = GetAlgoInfo(genAlgo);
	if (optAutoExtension) outputFile += selectedHashInfo->DefaultExt;

	// If only one file is selected then offer it's name as base for hash file name
	if (pi.SelectedItemsNumber == 1)
	{
		wstring strSelectedFile;
		if (GetSelectedPanelItemPath(strSelectedFile) && (strSelectedFile != L".."))
		{
			outputFile = ExtractFileName(strSelectedFile) + selectedHashInfo->DefaultExt;
		}
	}

	while(true)
	{
		if (!AskForHashGenerationParams(genAlgo, recursive, outputTarget, outputFile, storeAbsPaths))
			return;

		// Check if hash file already exists
		if ((outputTarget == OT_SINGLEFILE) && IsFile(outputFile.c_str()))
		{
			wchar_t wszMsgText[100];
			swprintf_s(wszMsgText, ARRAY_SIZE(wszMsgText), GetLocMsg(MSG_DLG_OVERWRITE_FILE_TEXT), outputFile.c_str());

			if (!ConfirmMessage(GetLocMsg(MSG_DLG_OVERWRITE_FILE), wszMsgText, true))
				continue;
		}

		break;
	}

	StringList filesToProcess;
	int64_t totalFilesSize = 0;
	HashList hashes;
	wstring strPanelDir;

	// Win7 only feature
	FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_SETPROGRESSSTATE, (void*) PS_INDETERMINATE);

	// Prepare files list
	{
		FarScreenSave screen;
		DisplayMessage(GetLocMsg(MSG_DLG_PROCESSING), GetLocMsg(MSG_DLG_PREPARE_LIST), NULL, false, false);

		GetPanelDir(PANEL_ACTIVE, strPanelDir);
		GetSelectedPanelFiles(pi, strPanelDir, filesToProcess, totalFilesSize, recursive);
	}

	// Perform hashing
	char hashValueBuf[150] = {0};
	ProgressContext progressCtx;
	progressCtx.TotalFilesCount = (int) filesToProcess.size();
	progressCtx.TotalFilesSize = totalFilesSize;
	progressCtx.TotalProcessedBytes = 0;
	progressCtx.CurrentFileIndex = -1;

	bool continueSave = true;
	bool fAutoSkipErrors = false;
	for (StringList::const_iterator cit = filesToProcess.begin(); cit != filesToProcess.end(); cit++)
	{
		wstring strNextFile = *cit;
		wstring strFullPath = strPanelDir + strNextFile;
		bool fSaveHash = true;

		progressCtx.FileName = strNextFile;
		progressCtx.CurrentFileIndex++;
		progressCtx.CurrentFileSize = GetFileSize_i64(strFullPath.c_str());

		int nOldTotalProgress = progressCtx.TotalProgress;
		int64_t nOldTotalBytes = progressCtx.TotalProcessedBytes;

		{
			FarScreenSave screen;
			
			while(true)
			{
				progressCtx.FileProgress = 0;
				progressCtx.CurrentFileProcessedBytes = 0;
				progressCtx.TotalProgress = nOldTotalProgress;
				progressCtx.TotalProcessedBytes = nOldTotalBytes;
				
				fSaveHash = true;

				int genRetVal = GenerateHash(strFullPath.c_str(), genAlgo, hashValueBuf, optHashUppercase != 0, FileHashingProgress, &progressCtx);

				if (genRetVal == GENERATE_ABORTED)
				{
					// Exit silently
					continueSave = false;
				}
				else if (genRetVal == GENERATE_ERROR)
				{
					int resp = fAutoSkipErrors ? EDR_SKIP : DisplayHashGenerateError(strNextFile);
					if (resp == EDR_RETRY)
						continue;
					else if (resp == EDR_SKIP)
						fSaveHash = false;
					else if (resp == EDR_SKIPALL)
					{
						fSaveHash = false;
						fAutoSkipErrors = true;
					}
					else
						continueSave = false;
				}

				// Always break if not said otherwise
				break;
			}
		}

		if (!continueSave) break;

		if (fSaveHash)
		{
			hashes.SetFileHash(storeAbsPaths ? strFullPath.c_str() : strNextFile.c_str(), hashValueBuf, genAlgo);
		}
	}

	FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_SETPROGRESSSTATE, (void*) PS_NOPROGRESS);
	FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_PROGRESSNOTIFY, 0);

	if (!continueSave) return;

	// Display/save hash list
	bool saveSuccess = false;
	if (outputTarget == OT_SINGLEFILE)
	{
		saveSuccess = hashes.SaveList(outputFile.c_str());
	}
	else if (outputTarget == OT_SEPARATEFILES)
	{
		saveSuccess = hashes.SaveListSeparate(strPanelDir.c_str());
	}
	else
	{
		saveSuccess = true;
		DisplayHashListOnScreen(hashes);
	}

	// Clear selection if requested
	if (saveSuccess && optClearSelectionOnComplete)
	{
		for (int i = pi.SelectedItemsNumber - 1; i >=0; i--)
			FarSInfo.Control(PANEL_ACTIVE, FCTL_CLEARSELECTION, i, NULL);
	}

	FarSInfo.Control(PANEL_ACTIVE, FCTL_REDRAWPANEL, 0, NULL);
}

static bool AskForCompareParams(rhash_ids &selectedAlgo, bool &recursive)
{
	FarDialogItem DialogItems []={
		/*0*/{DI_DOUBLEBOX,		3, 1, 41,13, 0, 0, 0, 0, GetLocMsg(MSG_DLG_COMPARE)},

		/*1*/{DI_TEXT,			5, 2, 0, 0, 0, 0, 0, 0, GetLocMsg(MSG_GEN_ALGO), 0},
		/*2*/{DI_RADIOBUTTON,	6, 3, 0, 0, 0, (selectedAlgo==RHASH_CRC32), DIF_GROUP, 0, L"&1. CRC32"},
		/*3*/{DI_RADIOBUTTON,	6, 4, 0, 0, 0, (selectedAlgo==RHASH_MD5), 0, 0, L"&2. MD5"},
		/*4*/{DI_RADIOBUTTON,	6, 5, 0, 0, 0, (selectedAlgo==RHASH_SHA1), 0, 0, L"&3. SHA1"},
		/*5*/{DI_RADIOBUTTON,	6, 6, 0, 0, 0, (selectedAlgo==RHASH_SHA256), 0, 0, L"&4. SHA256"},
		/*6*/{DI_RADIOBUTTON,	6, 7, 0, 0, 0, (selectedAlgo==RHASH_SHA512), 0, 0, L"&5. SHA512"},
		/*7*/{DI_RADIOBUTTON,	6, 8, 0, 0, 0, (selectedAlgo==RHASH_WHIRLPOOL), 0, 0, L"&6. Whirlpool"},

		/*8*/{DI_TEXT,			3, 9, 0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L""},
		/*9*/{DI_CHECKBOX,		5,10, 0, 0, 0, recursive, 0, 0, GetLocMsg(MSG_GEN_RECURSE)},

		/*10*/{DI_TEXT,			3,11, 0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L"", 0},
		/*11*/{DI_BUTTON,		0,12, 0,13, 0, 0, DIF_CENTERGROUP, 1, GetLocMsg(MSG_BTN_RUN), 0},
		/*12*/{DI_BUTTON,		0,12, 0,13, 0, 0, DIF_CENTERGROUP, 0, GetLocMsg(MSG_BTN_CANCEL), 0},
	};
	size_t numDialogItems = sizeof(DialogItems) / sizeof(DialogItems[0]);

	HANDLE hDlg = FarSInfo.DialogInit(FarSInfo.ModuleNumber, -1, -1, 45, 15, L"CompareParams", DialogItems, (unsigned) numDialogItems, 0, 0, FarSInfo.DefDlgProc, 0);

	bool retVal = false;
	if (hDlg != INVALID_HANDLE_VALUE)
	{
		int ExitCode = FarSInfo.DialogRun(hDlg);
		if (ExitCode == numDialogItems - 2) // OK was pressed
		{
			recursive = DlgHlp_GetSelectionState(hDlg, 15) != 0;

			for (int i = 0; i < NUMBER_OF_SUPPORTED_HASHES; i++)
			{
				// Selection radios start from index = 2
				if (DlgHlp_GetSelectionState(hDlg, 2 + i))
					selectedAlgo = SupportedHashes[i].AlgoId;
			}

			retVal = true;
		}
		FarSInfo.DialogFree(hDlg);
	}
	return retVal;
}

static bool RunGeneration(const wstring& filePath, rhash_ids hashAlgo, ProgressContext& progressCtx, char* hashStrBuffer, bool &shouldAbort, bool &shouldSkipAllErrors)
{
	FarScreenSave screen;

	progressCtx.FileName = filePath;
	progressCtx.CurrentFileIndex++;
	progressCtx.CurrentFileSize = GetFileSize_i64(filePath.c_str());

	int nOldTotalProgress = progressCtx.TotalProgress;
	int64_t nOldTotalBytes = progressCtx.TotalProcessedBytes;

	shouldAbort = false;

	while (true)
	{
		progressCtx.FileProgress = 0;
		progressCtx.CurrentFileProcessedBytes = 0;
		progressCtx.TotalProgress = nOldTotalProgress;
		progressCtx.TotalProcessedBytes = nOldTotalBytes;

		// Next is hash calculation for both files
		int genRetVal = GenerateHash(filePath.c_str(), hashAlgo, hashStrBuffer, false, FileHashingProgress, &progressCtx);

		if (genRetVal == GENERATE_ABORTED)
		{
			// Exit silently
			shouldAbort = true;
			return false;
		}
		else if (genRetVal == GENERATE_ERROR)
		{
			int errResp = shouldSkipAllErrors ? EDR_SKIP : DisplayHashGenerateError(filePath);
			if (errResp == EDR_RETRY)
				continue;

			shouldAbort = (errResp == EDR_ABORT);
			shouldSkipAllErrors = shouldSkipAllErrors || (errResp == EDR_SKIPALL);

			return false;
		}
		
		break;
	}

	return true;
}

static void RunComparePanels()
{
	PanelInfo piActv, piPasv;
	if (!FarSInfo.Control(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, (LONG_PTR) &piActv)
		|| !FarSInfo.Control(PANEL_PASSIVE, FCTL_GETPANELINFO, 0, (LONG_PTR) &piPasv))
		return;
	
	if (piActv.PanelType != PTYPE_FILEPANEL || piPasv.PanelType != PTYPE_FILEPANEL || piActv.Plugin || piPasv.Plugin)
	{
		DisplayMessage(GetLocMsg(MSG_DLG_ERROR), GetLocMsg(MSG_DLG_FILE_PANEL_REQUIRED), NULL, true, true);
		return;
	}

	// Nothing selected on the panel
	if (piActv.SelectedItemsNumber == 0) return;

	wstring strActivePanelDir, strPassivePanelDir;
	StringList vSelectedFiles;
	int64_t totalFilesSize = 0;

	rhash_ids cmpAlgo = (rhash_ids) optDefaultAlgo;
	bool recursive = true;

	GetPanelDir(PANEL_ACTIVE, strActivePanelDir);
	GetPanelDir(PANEL_PASSIVE, strPassivePanelDir);

	if (strActivePanelDir == strPassivePanelDir)
	{
		DisplayMessage(GetLocMsg(MSG_DLG_ERROR), GetLocMsg(MSG_DLG_NO_COMPARE_SELF), NULL, true, true);
		return;
	}

	if (!AskForCompareParams(cmpAlgo, recursive))
		return;

	FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_SETPROGRESSSTATE, (void*) PS_INDETERMINATE);
		
	// Prepare files list
	{
		FarScreenSave screen;
		DisplayMessage(GetLocMsg(MSG_DLG_PROCESSING), GetLocMsg(MSG_DLG_PREPARE_LIST), NULL, false, false);

		GetSelectedPanelFiles(piActv, strActivePanelDir, vSelectedFiles, totalFilesSize, true);
	}

	// No suitable items selected for comparison
	if (vSelectedFiles.size() == 0) return;

	vector<wstring> vMismatches, vMissing;
	int nFilesSkipped = 0;
	char szHashValueActive[130] = {0};
	char szHashValuePassive[130] = {0};
	bool fAborted = false;
	bool fSkipAllErrors = false;

	ProgressContext progressCtx;
	progressCtx.TotalFilesCount = (int) vSelectedFiles.size() * 2;
	progressCtx.TotalFilesSize = totalFilesSize * 2;
	progressCtx.TotalProcessedBytes = 0;
	progressCtx.CurrentFileIndex = -1;

	for (StringList::const_iterator cit = vSelectedFiles.begin(); cit != vSelectedFiles.end(); cit++)
	{
		wstring strNextFile = *cit;

		wstring strActvPath = strActivePanelDir + strNextFile;
		wstring strPasvPath = strPassivePanelDir + strNextFile;

		int64_t nActivePanelFileSize = GetFileSize_i64(strActvPath.c_str());

		// Does opposite file exists at all?
		if (!IsFile(strPasvPath.c_str()))
		{
			vMissing.push_back(strNextFile);
			progressCtx.CurrentFileIndex += 2;
			progressCtx.TotalProcessedBytes += nActivePanelFileSize * 2;
			continue;
		}

		// For speed compare file sizes first
		if (nActivePanelFileSize != GetFileSize_i64(strPasvPath.c_str()))
		{
			vMismatches.push_back(strNextFile);
			progressCtx.CurrentFileIndex += 2;
			progressCtx.TotalProcessedBytes += nActivePanelFileSize * 2;
			continue;
		}

		if (RunGeneration(strActvPath, cmpAlgo, progressCtx, szHashValueActive, fAborted, fSkipAllErrors)
			&& RunGeneration(strPasvPath, cmpAlgo, progressCtx, szHashValuePassive, fAborted, fSkipAllErrors))
		{
			if (strcmp(szHashValueActive, szHashValuePassive) != 0)
				vMismatches.push_back(strNextFile);
		}
		else
		{
			if (fAborted)
				break;
			else
				nFilesSkipped++;
		}
	}

	FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_SETPROGRESSSTATE, (void*) PS_NOPROGRESS);
	FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_PROGRESSNOTIFY, 0);

	if (!fAborted)
	{
		DisplayValidationResults(vMismatches, vMissing, nFilesSkipped);
	}
}

void RunCompareWithClipboard(std::wstring &selectedFile)
{
	std::string clipText;
	if (!GetTextFromClipboard(clipText))
	{
		DisplayMessage(GetLocMsg(MSG_DLG_ERROR), GetLocMsg(MSG_DLG_CLIP_ERROR), NULL, true, true);
		return;
	}

	TrimStr(clipText);

	std::vector<int> algoIndicies = DetectHashAlgo(clipText);

	if (algoIndicies.size() == 0)
	{
		DisplayMessage(GetLocMsg(MSG_DLG_ERROR), GetLocMsg(MSG_DLG_LOOKS_NO_HASH), NULL, true, true);
		return;
	}

	int selectedAlgoIndex = algoIndicies[0];
	if (algoIndicies.size() > 1)
	{
		FarMenu algoMenu(&FarSInfo, L"Select algorithm");
		for (size_t i = 0; i < algoIndicies.size(); i++)
		{
			algoMenu.AddItem(SupportedHashes[algoIndicies[i]].AlgoName.c_str());
		}
		int selItem = algoMenu.Run();
		if (selItem < 0) return;

		selectedAlgoIndex = algoIndicies[selItem];
	}

	rhash_ids algo = SupportedHashes[selectedAlgoIndex].AlgoId;

	char szHashValueBuf[150] = {0};
	bool fAborted = false, fSkipAllErrors = false;

	ProgressContext progressCtx;
	progressCtx.TotalFilesCount = 1;
	progressCtx.TotalFilesSize = GetFileSize_i64(selectedFile.c_str());
	progressCtx.TotalProcessedBytes = 0;
	progressCtx.CurrentFileIndex = -1;

	if (RunGeneration(selectedFile, algo, progressCtx, szHashValueBuf, fAborted, fSkipAllErrors))
	{
		if (_stricmp(szHashValueBuf, clipText.c_str()) == 0)
			DisplayMessage(GetLocMsg(MSG_DLG_CALC_COMPLETE), GetLocMsg(MSG_DLG_FILE_CLIP_MATCH), NULL, false, true);
		else
			DisplayMessage(GetLocMsg(MSG_DLG_CALC_COMPLETE), GetLocMsg(MSG_DLG_FILE_CLIP_MISMATCH), NULL, true, true);
	}
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

	static const wchar_t *PluginMenuStrings[1];
	PluginMenuStrings[0] = GetLocMsg(MSG_PLUGIN_NAME);
	static const wchar_t *PluginConfigStrings[1];
	PluginConfigStrings[0] = GetLocMsg(MSG_PLUGIN_CONFIG_NAME);

	Info->PluginMenuStrings = PluginMenuStrings;
	Info->PluginMenuStringsNumber = sizeof(PluginMenuStrings) / sizeof(PluginMenuStrings[0]);
	Info->PluginConfigStrings = PluginConfigStrings;
	Info->PluginConfigStringsNumber = sizeof(PluginConfigStrings) / sizeof(PluginConfigStrings[0]);
	Info->CommandPrefix = optPrefix;
}

int WINAPI ConfigureW(int ItemNumber)
{
	FarListItem algoListItems[NUMBER_OF_SUPPORTED_HASHES] = {0};
	FarList algoDlgList = {NUMBER_OF_SUPPORTED_HASHES, algoListItems};

	FarDialogItem DialogItems []={
		/*00*/ {DI_DOUBLEBOX, 3, 1,40,13, 0, 0, 0,0, GetLocMsg(MSG_CONFIG_TITLE), 0},
		/*01*/ {DI_TEXT,	  5, 2, 0, 0, 0, 0, 0, 0, GetLocMsg(MSG_CONFIG_DEFAULT_ALGO), 0},
		/*02*/ {DI_COMBOBOX,  5, 3,20, 0, 0, (DWORD_PTR)&algoDlgList, DIF_DROPDOWNLIST, 0, NULL, 0},
		/*03*/ {DI_TEXT,	  3, 4, 0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L"", 0},
		/*04*/ {DI_CHECKBOX,  5, 5, 0, 0, 0, optUsePrefix, 0,0, GetLocMsg(MSG_CONFIG_PREFIX), 0},
		/*05*/ {DI_EDIT,	  8, 6,24, 0, 0, 0, 0,0, optPrefix, 0},
		/*06*/ {DI_CHECKBOX,  5, 7, 0, 0, 0, optConfirmAbort, 0,0, GetLocMsg(MSG_CONFIG_CONFIRM_ABORT), 0},
		/*07*/ {DI_CHECKBOX,  5, 8, 0, 0, 0, optClearSelectionOnComplete, 0,0, GetLocMsg(MSG_CONFIG_CLEAR_SELECTION), 0},
		/*08*/ {DI_CHECKBOX,  5, 9, 0, 0, 0, optAutoExtension, 0,0, GetLocMsg(MSG_CONFIG_AUTOEXT), 0},
		/*09*/ {DI_CHECKBOX,  5,10, 0, 0, 0, optHashUppercase, 0,0, GetLocMsg(MSG_CONFIG_UPPERCASE), 0},
		/*10*/ {DI_TEXT,	  3,11, 0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L"", 0},
		/*11*/ {DI_BUTTON,	  0,12, 0, 0, 0, 0, DIF_CENTERGROUP, 1, GetLocMsg(MSG_BTN_OK), 0},
		/*12*/ {DI_BUTTON,    0,12, 0, 0, 1, 0, DIF_CENTERGROUP, 0, GetLocMsg(MSG_BTN_CANCEL), 0},
	};

	for (int i = 0; i < NUMBER_OF_SUPPORTED_HASHES; i++)
	{
		algoListItems[i].Text = SupportedHashes[i].AlgoName.c_str();
		if (SupportedHashes[i].AlgoId == optDefaultAlgo)
			algoListItems[i].Flags = LIF_SELECTED;
	}

	// Expand right border of the dialog if test is too long to fit
	int borderX2 = DialogItems[0].X2;
	for (int i = 1; i < ARRAY_SIZE(DialogItems); i++)
	{
		if (DialogItems[i].Type == DI_CHECKBOX)
		{
			int itemRigthBorder = DialogItems[i].X1 + 4 + wcslen(DialogItems[i].PtrData) + 1;
			if (itemRigthBorder > borderX2)
			{
				borderX2 = itemRigthBorder;
				DialogItems[0].X2 = itemRigthBorder;
			}
		}
	}

	HANDLE hDlg = FarSInfo.DialogInit(FarSInfo.ModuleNumber, -1, -1, borderX2 + 4, 15, L"IntCheckerConfig",
		DialogItems, sizeof(DialogItems) / sizeof(DialogItems[0]), 0, 0, FarSInfo.DefDlgProc, 0);

	int nOkID = ARRAY_SIZE(DialogItems) - 2;

	if (hDlg != INVALID_HANDLE_VALUE)
	{
		int ExitCode = FarSInfo.DialogRun(hDlg);
		if (ExitCode == nOkID) // OK was pressed
		{
			optUsePrefix = DlgHlp_GetSelectionState(hDlg, 4);
			DlgHlp_GetEditBoxText(hDlg, 5, optPrefix, ARRAY_SIZE(optPrefix));
			optConfirmAbort = DlgHlp_GetSelectionState(hDlg, 6);
			optClearSelectionOnComplete = DlgHlp_GetSelectionState(hDlg, 7);
			optAutoExtension = DlgHlp_GetSelectionState(hDlg, 8);
			optHashUppercase = DlgHlp_GetSelectionState(hDlg, 9);
			
			int selectedAlgo = (int) DlgList_GetCurPos(FarSInfo, hDlg, 2);
			optDefaultAlgo = SupportedHashes[selectedAlgo].AlgoId;

			SaveSettings();
		}
		FarSInfo.DialogFree(hDlg);

		if (ExitCode == nOkID) return TRUE;
	}

	return FALSE;
}

HANDLE WINAPI OpenPluginW(int OpenFrom, INT_PTR Item)
{
	if (OpenFrom == OPEN_COMMANDLINE)
	{
		if (optUsePrefix)
		{
			// We are from prefix
			if (!RunValidateFiles((wchar_t*) Item, true))
				DisplayMessage(GetLocMsg(MSG_DLG_ERROR), GetLocMsg(MSG_DLG_NOTVALIDLIST), NULL, true, true);
		}
	}
	else if (OpenFrom == OPEN_PLUGINSMENU)
	{
		// We are from regular plug-ins menu

		PanelInfo pi = {0};
		if (!FarSInfo.Control(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, (LONG_PTR)&pi) || (pi.PanelType != PTYPE_FILEPANEL) || (pi.Plugin != 0))
		{
			return INVALID_HANDLE_VALUE;
		}

		FarMenu openMenu(&FarSInfo, GetLocMsg(MSG_PLUGIN_NAME));

		openMenu.AddItemEx(GetLocMsg(MSG_MENU_GENERATE), boost::bind(RunGenerateHashes));
		openMenu.AddItemEx(GetLocMsg(MSG_MENU_COMPARE), boost::bind(RunComparePanels));

		wstring selectedFilePath;
		if ((pi.SelectedItemsNumber == 1) && GetSelectedPanelItemPath(selectedFilePath) && IsFile(selectedFilePath.c_str()))
		{
			//TODO: use optDetectHashFiles
			openMenu.AddItemEx(GetLocMsg(MSG_MENU_VALIDATE), boost::bind(RunValidateFiles, selectedFilePath.c_str(), false));
			openMenu.AddItemEx(GetLocMsg(MSG_MENU_COMPARE_CLIP), boost::bind(RunCompareWithClipboard, selectedFilePath));
		}

		openMenu.RunEx();
	} // OpenFrom check
		
	return INVALID_HANDLE_VALUE;
}

HANDLE WINAPI OpenFilePluginW(const wchar_t *Name, const unsigned char *Data, int DataSize, int OpMode)
{
	return INVALID_HANDLE_VALUE;
}
