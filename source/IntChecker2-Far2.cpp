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
					PrepareFilesList(strSelectedDir.c_str(), PPI->FindData.lpwszFileName, vDest, totalSize, recursive, nullptr, INVALID_HANDLE_VALUE);
				}
			}
			free(PPI);
		}
	}
}

static bool GetFarWindowSize(RectSize &size)
{
	SMALL_RECT farRect;
	if (FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_GETFARRECT, &farRect))
	{
		size.Assign(farRect);
		return true;
	}

	return false;
}

static int AdjustDialogBorder(FarDialogItem *DialogItems, int NumItems)
{
	// Expand right border of the dialog if text is too long to fit
	int borderX2 = DialogItems[0].X2;
	for (int i = 1; i < NumItems; i++)
	{
		if (DialogItems[i].Type == DI_CHECKBOX)
		{
			int itemRigthBorder = DialogItems[i].X1 + 4 + (int) wcslen(DialogItems[i].PtrData) + 1;
			if (itemRigthBorder > borderX2)
			{
				borderX2 = itemRigthBorder;
				DialogItems[0].X2 = itemRigthBorder;
			}
		}
		else if (DialogItems[i].Type == DI_COMBOBOX)
		{
			int rightBorder = DialogItems[i].X2 + 2;
			if (rightBorder > borderX2)
			{
				borderX2 = rightBorder;
				DialogItems[0].X2 = rightBorder;
			}
		}
	}

	return borderX2;
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
		regOpts.GetValue(L"RememberLastAlgorithm", optRememberLastUsedAlgo);
		regOpts.GetValue(L"DefaultListCodepage", optListDefaultCodepage);
		regOpts.GetValue(L"DefaultOutput", optDefaultOutputTarget);
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
		regOpts.SetValue(L"RememberLastAlgorithm", optRememberLastUsedAlgo);
		regOpts.SetValue(L"DefaultListCodepage", optListDefaultCodepage);
		regOpts.SetValue(L"DefaultOutput", optDefaultOutputTarget);
	}
}

static std::wstring FileSizeToString(int64_t fileSize, bool keepBytes)
{
	//todo: implement analog for FSF.FormatFileSize from Far3

	std::wstring result = FormatString(L"%lld", fileSize);

	if (keepBytes)
	{
		int cnt = 0;
		for (size_t i = result.length() - 1; i > 0; --i)
		{
			cnt++;
			if (cnt % 3 == 0)
				result.insert(i, 1, ' ');
		}
	}

	return result;
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
	if (prCtx->IncreaseProcessedBytes(bytesProcessed))
	{
		int nFileProgress = prCtx->GetCurrentFileProgress();
		int nTotalProgress = prCtx->GetTotalProgress();

		static wchar_t szGeneratingLine[128] = { 0 };
		swprintf_s(szGeneratingLine, ARRAY_SIZE(szGeneratingLine), GetLocMsg(MSG_DLG_CALCULATING), prCtx->HashAlgoName.c_str());

		std::wstring strFilesNumLine = JoinProgressLine(GetLocMsg(MSG_DLG_PROGRESS_NUMFILES), FormatString(L"%d / %d", prCtx->CurrentFileIndex + 1, prCtx->TotalFilesCount), cntProgressDialogWidth, 5);
		std::wstring strBytesLine = JoinProgressLine(GetLocMsg(MSG_DLG_PROGRESS_NUMBYTES), FileSizeToString(prCtx->TotalProcessedBytes, true) + L" / " + FileSizeToString(prCtx->TotalFilesSize, true), cntProgressDialogWidth, 5);

		std::wstring strPBarCurrent = ProgressBarString(nFileProgress, cntProgressDialogWidth);
		std::wstring strPBarTotal = ProgressBarString(nTotalProgress, cntProgressDialogWidth);

		int64_t elapsedTime = prCtx->GetElapsedTimeMS();
		int64_t avgSpeed = (elapsedTime > 0) ? (prCtx->TotalProcessedBytes * 1000) / elapsedTime : 0;

		std::wstring elapsedTimeStr = L"Time: " + DurationToString(elapsedTime);
		//std::wstring avgSpeedStr = FileSizeToString(avgSpeed, false) + L"/s";
		std::wstring avgSpeedStr = L""; //TODO: reenable speed display after proper formatting function is found
		std::wstring strSpeed = JoinProgressLine(elapsedTimeStr, avgSpeedStr, cntProgressDialogWidth, 0);

		static const wchar_t* InfoLines[10];
		InfoLines[0] = GetLocMsg(MSG_DLG_PROCESSING);
		InfoLines[1] = szGeneratingLine;
		InfoLines[2] = prCtx->GetShortenedPath();
		InfoLines[3] = strPBarCurrent.c_str();
		InfoLines[4] = L"\1";
		InfoLines[5] = strFilesNumLine.c_str();
		InfoLines[6] = strBytesLine.c_str();
		InfoLines[7] = strPBarTotal.c_str();
		InfoLines[8] = L"\1";
		InfoLines[9] = strSpeed.c_str();

		FarSInfo.Message(FarSInfo.ModuleNumber, 0, NULL, InfoLines, ARRAY_SIZE(InfoLines), 0);

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

static void SelectFilesOnPanel(HANDLE hPanel, vector<wstring> &fileNames, bool exclusive)
{
	if (!exclusive && (fileNames.size() == 0)) return;
	
	PanelInfo pi = {0};
	FarSInfo.Control(hPanel, FCTL_GETPANELINFO, 0, (LONG_PTR)&pi);

	FarSInfo.Control(hPanel, FCTL_BEGINSELECTION, 0, NULL);
	for (int i = 0; i < pi.ItemsNumber; i++)
	{
		PluginPanelItem *PPI = (PluginPanelItem*) malloc(FarSInfo.Control(hPanel, FCTL_GETPANELITEM, i, NULL));
		if (PPI)
		{
			FarSInfo.Control(hPanel, FCTL_GETPANELITEM, i, (LONG_PTR)PPI);
			bool isNameInList = std::find(fileNames.begin(), fileNames.end(), PPI->FindData.lpwszFileName) != fileNames.end();
			if (isNameInList || exclusive)
			{
				FarSInfo.Control(hPanel, FCTL_SETSELECTION, i, isNameInList ? TRUE : FALSE);
			}
			free(PPI);
		}
	}
	FarSInfo.Control(hPanel, FCTL_ENDSELECTION, 0, NULL);
	FarSInfo.Control(hPanel, FCTL_REDRAWPANEL, 0, NULL);
}

static int DisplayHashGenerateError(const std::wstring& fileName)
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

static bool RunGeneration(const wstring& filePath, const std::wstring& fileDisplayPath, rhash_ids hashAlgo, bool useHashUppercase, ProgressContext& progressCtx, std::string& hashStr, bool &shouldAbort, bool &shouldSkipAllErrors)
{
	progressCtx.NextFile(fileDisplayPath, GetFileSize_i64(filePath.c_str()));
	progressCtx.SetAlgorithm(hashAlgo);

	shouldAbort = false;

	while (true)
	{
		progressCtx.RestartFile();

		// Next is hash calculation for both files
		GenResult genRetVal = GenerateHash(filePath, hashAlgo, hashStr, useHashUppercase, FileHashingProgress, &progressCtx);

		if (genRetVal == GenResult::Aborted)
		{
			// Exit silently
			shouldAbort = true;
			return false;
		}
		else if (genRetVal == GenResult::Error)
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

static void DisplayValidationResults(std::vector<std::wstring> &vMismatchList, std::vector<std::wstring> &vMissingList, int numSkipped)
{
	if (vMismatchList.size() == 0 && vMissingList.size() == 0)
	{
		// If everything is fine then just display simple message
		static wchar_t wszGoodMessage[256];
		if (numSkipped == 0)
			wcscpy_s(wszGoodMessage, ARRAY_SIZE(wszGoodMessage), GetLocMsg(MSG_DLG_NO_MISMATCHES));
		else
			swprintf_s(wszGoodMessage, ARRAY_SIZE(wszGoodMessage), L"%s (%d %s)", GetLocMsg(MSG_DLG_NO_MISMATCHES), numSkipped, GetLocMsg(MSG_DLG_NUM_SKIPPED));

		DisplayMessage(GetLocMsg(MSG_DLG_VALIDATION_COMPLETE), wszGoodMessage, NULL, false, true);
	}
	else
	{
		// Otherwise display proper list of invalid/missing files

		vector<wstring> displayStrings;

		size_t nListIndex = 0;
		if (vMismatchList.size() > 0)
		{
			displayStrings.push_back(FormatString(GetLocMsg(MSG_DLG_MISMATCHED_FILES), vMismatchList.size()));

			for (size_t i = 0; i < vMismatchList.size(); i++)
			{
				wstring &nextFile = vMismatchList[i];
				displayStrings.push_back(FormatString(L"  %s", nextFile.c_str()));
			}
		}
		if (vMissingList.size() > 0)
		{
			displayStrings.push_back(FormatString(GetLocMsg(MSG_DLG_MISSING_FILES), vMissingList.size()));

			for (size_t i = 0; i < vMissingList.size(); i++)
			{
				wstring &nextFile = vMissingList[i];
				displayStrings.push_back(FormatString(L"  %s", nextFile.c_str()));
			}
		}
		
		size_t nNumListItems = displayStrings.size();
		FarListItem* mmListItems = (FarListItem*) malloc(nNumListItems * sizeof(FarListItem));
		FarList mmList = {(int)nNumListItems, mmListItems};
		memset(mmListItems, 0, nNumListItems * sizeof(FarListItem));

		RectSize listSize(57, 14);
		FindBestListBoxSize(displayStrings, GetFarWindowSize, listSize);

		for (size_t i = 0; i < nNumListItems; i++)
		{
			mmListItems[i].Text = displayStrings[i].c_str();
		}

		int nDlgWidth = listSize.Width + 11;
		int nDlgHeight = listSize.Height + 7;

		FarDialogItem DialogItems []={
			/*00*/ {DI_DOUBLEBOX, 3, 1,nDlgWidth-4,nDlgHeight-2, 0, 0, 0,0, GetLocMsg(MSG_DLG_VALIDATION_COMPLETE), 0},
			/*01*/ {DI_LISTBOX,   5, 2,nDlgWidth-6,nDlgHeight-5, 0, (DWORD_PTR) &mmList, DIF_LISTNOCLOSE | DIF_LISTNOBOX, 0, NULL, 0},
			/*02*/ {DI_TEXT,	  3,nDlgHeight-4, 0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L"", 0},
			/*03*/ {DI_BUTTON,	  0,nDlgHeight-3, 0, 0, 1, 0, DIF_CENTERGROUP, 1, GetLocMsg(MSG_BTN_CLOSE), 0},
			/*04*/ {DI_BUTTON,    0,nDlgHeight-3, 0, 0, 0, 0, DIF_CENTERGROUP, 0, GetLocMsg(MSG_BTN_CLIPBOARD), 0},
		};

		HANDLE hDlg = FarSInfo.DialogInit(FarSInfo.ModuleNumber, -1, -1, nDlgWidth, nDlgHeight, NULL,
			DialogItems, sizeof(DialogItems) / sizeof(DialogItems[0]), 0, 0, FarSInfo.DefDlgProc, 0);
		
		int ExitCode = FarSInfo.DialogRun(hDlg);
		if (ExitCode == 4) // clipboard
		{
			CopyTextToClipboard(displayStrings);
		}
		FarSInfo.DialogFree(hDlg);
		free(mmListItems);

		// Select mismatched files that are in the same folder
		std::vector<std::wstring> vSameFolderFiles;
		for (size_t i = 0; i < vMismatchList.size(); i++)
		{
			wstring &nextFile = vMismatchList[i];
			if (nextFile.find_first_of(L"\\/") == wstring::npos)
				vSameFolderFiles.push_back(nextFile);
		}
		SelectFilesOnPanel(PANEL_ACTIVE, vSameFolderFiles, true);
	}
}

static bool AskValidationFileParams(UINT &codepage, int &ignoreMissingFiles)
{
	const wchar_t* codePageNames[] = {L"UTF-8", L"ANSI", L"OEM"};
	const UINT codePageValues[] = {CP_UTF8, CP_ACP, CP_OEMCP};

	FarListItem cpListItems[ARRAY_SIZE(codePageNames)] = {0};
	FarList cpDlgList = {ARRAY_SIZE(codePageNames), cpListItems};

	FarDialogItem DialogItems[] = {
		/*00*/ { DI_DOUBLEBOX, 3, 1,30, 6, 0, 0, 0,0, GetLocMsg(MSG_MENU_VALIDATE), 0},
		/*01*/ { DI_TEXT,	   5, 2, 0, 0, 0, 0, 0, 0, GetLocMsg(MSG_GEN_CODEPAGE), 0},
		/*02*/ { DI_COMBOBOX,  5, 2, 0, 0, 0, (DWORD_PTR)&cpDlgList, DIF_DROPDOWNLIST, 0, NULL, 0},
		/*03*/ { DI_CHECKBOX,  5, 3, 0, 0, 0, (DWORD_PTR)ignoreMissingFiles, 0, 0, GetLocMsg(MSG_DLG_IGNORE_MISSING), 0 },
		/*04*/ { DI_TEXT,	   3, 4, 0, 0, 0, 0, DIF_BOXCOLOR | DIF_SEPARATOR, 0, L"", 0 },
		/*05*/ { DI_BUTTON,	   0, 5, 0, 0, 1, 0, DIF_CENTERGROUP, 1, GetLocMsg(MSG_BTN_RUN), 0},
		/*06*/ { DI_BUTTON,    0, 5, 0, 0, 0, 0, DIF_CENTERGROUP, 0, GetLocMsg(MSG_BTN_CANCEL), 0},
	};

	for (int i = 0; i < ARRAY_SIZE(codePageValues); i++)
	{
		cpListItems[i].Text = codePageNames[i];
		if (codePageValues[i] == optListDefaultCodepage)
			cpListItems[i].Flags = LIF_SELECTED;
	}

	// Set proper location for codepage combobox
	DialogItems[2].X1 += (int) wcslen(DialogItems[1].PtrData) + 1;
	DialogItems[2].X2 += DialogItems[2].X1 + 6;

	// Expand right border of the dialog if text is too long to fit
	int borderX2 = AdjustDialogBorder(DialogItems, ARRAY_SIZE(DialogItems));

	HANDLE hDlg = FarSInfo.DialogInit(FarSInfo.ModuleNumber, -1, -1, borderX2 + 4, 8, L"ValidationParams",
		DialogItems, sizeof(DialogItems) / sizeof(DialogItems[0]), 0, 0, FarSInfo.DefDlgProc, 0);

	int nOkID = ARRAY_SIZE(DialogItems) - 2;

	if (hDlg != INVALID_HANDLE_VALUE)
	{
		int ExitCode = FarSInfo.DialogRun(hDlg);
		if (ExitCode == nOkID) // OK was pressed
		{
			int selectedCodepage = (int) DlgList_GetCurPos(FarSInfo, hDlg, 2);
			codepage = codePageValues[selectedCodepage];
			ignoreMissingFiles = (int) DlgItem_GetCheck(FarSInfo, hDlg, 3);
		}
		FarSInfo.DialogFree(hDlg);

		if (ExitCode == nOkID) return true;
	}

	return false;
}

// Returns true if file is recognized as hash list
static bool RunValidateFiles(const wchar_t* hashListPath, bool silent, bool showParamsDialog)
{
	UINT fileCodepage = optListDefaultCodepage;
	int ignoreMissingFiles = optIgnoreMissingFiles;

	if (!silent && showParamsDialog && !AskValidationFileParams(fileCodepage, ignoreMissingFiles))
		return false;
	
	HashList hashes;
	if (!hashes.LoadList(hashListPath, fileCodepage, false) || (hashes.GetCount() == 0))
	{
		if (!silent)
			DisplayMessage(GetLocMsg(MSG_DLG_ERROR), GetLocMsg(MSG_DLG_NOTVALIDLIST), NULL, true, true);
		return false;
	}

	int nFilesSkipped = 0;
	std::vector<wstring> vMismatches, vMissing;
	std::vector<size_t> existingFiles;
	int64_t totalFilesSize = 0;

	// Win7 only feature
	FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_SETPROGRESSSTATE, (void*) PS_INDETERMINATE);

	// Prepare files list
	{
		FarScreenSave screen;
		DisplayMessage(GetLocMsg(MSG_DLG_PROCESSING), GetLocMsg(MSG_DLG_PREPARE_LIST), NULL, false, false);

		int64_t fileSize;
		for (size_t i = 0; i < hashes.GetCount(); i++)
		{
			FileHashInfo fileInfo = hashes.GetFileInfo(i);

			wstring strFullFilePath = ConvertPathToNative(fileInfo.Filename);
			if (IsFile(strFullFilePath, &fileSize))
			{
				existingFiles.push_back(i);
				totalFilesSize += fileSize;
			}
			else if (!ignoreMissingFiles)
			{
				vMissing.push_back(fileInfo.Filename);
			}
		}
	}

	if (existingFiles.size() > 0)
	{
		bool fAborted = false;
		{ // Scope for ProgressContext object
			ProgressContext progressCtx((int)existingFiles.size(), totalFilesSize);

			bool fAutoSkipErrors = false;
			for (size_t i = 0; i < existingFiles.size(); i++)
			{
				FileHashInfo fileInfo = hashes.GetFileInfo(existingFiles[i]);
				
				wstring strFullFilePath = ConvertPathToNative(fileInfo.Filename);
				std::string hashValueStr;

				if (RunGeneration(strFullFilePath, fileInfo.Filename, fileInfo.GetAlgo(), false, progressCtx, hashValueStr, fAborted, fAutoSkipErrors))
				{
					if (!SameHash(fileInfo.HashStr, hashValueStr))
						vMismatches.push_back(fileInfo.Filename);
				}
				else if (fAborted)
				{
					break;
				}
				else
				{
					nFilesSkipped++;
				}
			} //for
		}

		if (!fAborted)
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
	const int nFileNameBoxIndex = 14;
	
	if (Msg == DN_BTNCLICK && optAutoExtension)
	{
		if (Param2 && (Param1 >= 2) && (Param1 <= 2 + NUMBER_OF_SUPPORTED_HASHES))
		{
			int selectedHashIndex = Param1 - 2;
			wchar_t wszHashFileName[MAX_PATH];

			DlgHlp_GetEditBoxText(hDlg, nFileNameBoxIndex, wszHashFileName, ARRAY_SIZE(wszHashFileName));
			
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
						FarSInfo.SendDlgMessage(hDlg, DM_SETTEXTPTR, nFileNameBoxIndex, (LONG_PTR)wszHashFileName);
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
		/*0*/{DI_DOUBLEBOX,		3, 1, 45,21, 0, 0, 0, 0, GetLocMsg(MSG_GEN_TITLE)},

		/*1*/{DI_TEXT,			5, 2, 0, 0, 0, 0, 0, 0, GetLocMsg(MSG_GEN_ALGO), 0},
		/*2*/{DI_RADIOBUTTON,	6, 3, 0, 0, 0, (selectedAlgo == RHASH_CRC32), DIF_GROUP, 0, GetLocMsg(MSG_ALGO_CRC)},
		/*3*/{DI_RADIOBUTTON,	6, 4, 0, 0, 0, (selectedAlgo == RHASH_MD5), 0, 0, GetLocMsg(MSG_ALGO_MD5)},
		/*4*/{DI_RADIOBUTTON,	6, 5, 0, 0, 0, (selectedAlgo == RHASH_SHA1), 0, 0, GetLocMsg(MSG_ALGO_SHA1)},
		/*5*/{DI_RADIOBUTTON,	6, 6, 0, 0, 0, (selectedAlgo == RHASH_SHA256), 0, 0, GetLocMsg(MSG_ALGO_SHA256)},
		/*6*/{DI_RADIOBUTTON,	6, 7, 0, 0, 0, (selectedAlgo == RHASH_SHA512), 0, 0, GetLocMsg(MSG_ALGO_SHA512)},
		/*7*/{DI_RADIOBUTTON,   6, 8, 0, 0, 0, (selectedAlgo == RHASH_SHA3_512), 0, 0, GetLocMsg(MSG_ALGO_SHA3_512) },
		/*8*/{DI_RADIOBUTTON,	6, 9, 0, 0, 0, (selectedAlgo == RHASH_WHIRLPOOL), 0, 0, GetLocMsg(MSG_ALGO_WHIRLPOOL)},
		
		/*9*/{DI_TEXT,			3,10, 0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L""},
		/*10*/{DI_TEXT,			5,11, 0, 0, 0, 0, 0, 0, GetLocMsg(MSG_GEN_TARGET), 0},
		/*11*/{DI_RADIOBUTTON,	6,12, 0, 0, 0, 1, DIF_GROUP, 0, GetLocMsg(MSG_GEN_TO_FILE)},
		/*12*/{DI_RADIOBUTTON,	6,14, 0, 0, 0, 0, 0, 0, GetLocMsg(MSG_GEN_TO_SEPARATE)},
		/*13*/{DI_RADIOBUTTON,	6,15, 0, 0, 0, 0, 0, 0, GetLocMsg(MSG_GEN_TO_SCREEN)},
		/*14*/{DI_EDIT,		   10,13,38, 0, 1, 0, DIF_EDITEXPAND|DIF_EDITPATH,0, outputFileName.c_str(), 0},
		
		/*15*/{DI_TEXT,			3,16, 0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L""},
		/*16*/{DI_CHECKBOX,		5,17, 0, 0, 0, recursive, 0, 0, GetLocMsg(MSG_GEN_RECURSE)},
		/*17*/{DI_CHECKBOX,		5,18, 0, 0, 0, (DWORD_PTR)storeAbsPaths, 0, 0, GetLocMsg(MSG_GEN_ABSPATH)},
		
		/*18*/{DI_TEXT,			3,19, 0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L"", 0},
		/*19*/{DI_BUTTON,		0,20, 0,13, 0, 0, DIF_CENTERGROUP, 1, GetLocMsg(MSG_BTN_RUN), 0},
		/*20*/{DI_BUTTON,		0,20, 0,13, 0, 0, DIF_CENTERGROUP, 0, GetLocMsg(MSG_BTN_CANCEL), 0},
	};
	size_t numDialogItems = sizeof(DialogItems) / sizeof(DialogItems[0]);

	HANDLE hDlg = FarSInfo.DialogInit(FarSInfo.ModuleNumber, -1, -1, 49, 23, L"GenerateParams", DialogItems, (unsigned) numDialogItems, 0, 0, HashParamsDlgProc, 0);

	bool retVal = false;
	if (hDlg != INVALID_HANDLE_VALUE)
	{
		int ExitCode = FarSInfo.DialogRun(hDlg);
		if (ExitCode == numDialogItems - 2) // OK was pressed
		{
			recursive = DlgHlp_GetSelectionState(hDlg, 16) != 0;
			storeAbsPaths = DlgHlp_GetSelectionState(hDlg, 17);
			DlgHlp_GetEditBoxText(hDlg, 14, outputFileName);

			for (int i = 0; i < NUMBER_OF_SUPPORTED_HASHES; i++)
			{
				// Selection radios start from index = 2
				if (DlgHlp_GetSelectionState(hDlg, 2 + i))
					selectedAlgo = SupportedHashes[i].AlgoId;
			}

			if (DlgHlp_GetSelectionState(hDlg, 11)) outputTarget = OT_SINGLEFILE;
			else if (DlgHlp_GetSelectionState(hDlg, 12)) outputTarget = OT_SEPARATEFILES;
			else if (DlgHlp_GetSelectionState(hDlg, 13)) outputTarget = OT_DISPLAY;
			
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

	std::vector<std::wstring> listStrDump;
	for (size_t i = 0; i < list.GetCount(); i++)
	{
		listStrDump.push_back(list.GetFileInfo(i).ToString());

		wstring &line = listStrDump[i];
		memset(&hashListItems[i], 0, sizeof(FarListItem));
		hashListItems[i].Text = line.c_str();
	}

	RectSize listSize(54, 15);
	FindBestListBoxSize(listStrDump, GetFarWindowSize, listSize);

	int nDlgWidth = listSize.Width + 11;
	int nDlgHeight = listSize.Height + 7;

	FarDialogItem DialogItems []={
		/*00*/ {DI_DOUBLEBOX, 3, 1,nDlgWidth-4,nDlgHeight-2, 0, 0, 0,0, GetLocMsg(MSG_DLG_CALC_COMPLETE), 0},
		/*01*/ {DI_LISTBOX,   5, 2,listSize.Width+5,listSize.Height+2, 0, (DWORD_PTR)&hashDump, DIF_LISTNOCLOSE | DIF_LISTNOBOX, 0, NULL, 0},
		/*02*/ {DI_TEXT,	  3,nDlgHeight-4, 0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L"", 0},
		/*03*/ {DI_BUTTON,	  0,nDlgHeight-3, 0, 0, 1, 0, DIF_CENTERGROUP, 1, GetLocMsg(MSG_BTN_CLOSE), 0},
		/*04*/ {DI_BUTTON,    0,nDlgHeight-3, 0, 0, 0, 0, DIF_CENTERGROUP, 0, GetLocMsg(MSG_BTN_CLIPBOARD), 0},
	};

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
	UINT outputFileCodepage = optListDefaultCodepage;
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

		if (outputTarget == OT_SINGLEFILE)
		{
			// Check if hash file already exists
			if (IsFile(outputFile))
			{
				wchar_t wszMsgText[256] = {0};
				swprintf_s(wszMsgText, ARRAY_SIZE(wszMsgText), GetLocMsg(MSG_DLG_OVERWRITE_FILE_TEXT), outputFile.c_str());

				if (!ConfirmMessage(GetLocMsg(MSG_DLG_OVERWRITE_FILE), wszMsgText, true))
					continue;
			}
			// Check if we can write target file
			else if (!CanCreateFile(outputFile.c_str()))
			{
				DisplayMessage(MSG_DLG_ERROR, MSG_DLG_CANT_SAVE_HASHLIST, outputFile.c_str(), true, true);
				continue;
			}
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

	{
		// Perform hashing
		ProgressContext progressCtx((int)filesToProcess.size(), totalFilesSize);
		progressCtx.SetAlgorithm(genAlgo);

		bool continueSave = true;
		bool fAutoSkipErrors = false;
		for (auto cit = filesToProcess.begin(); cit != filesToProcess.end(); cit++)
		{
			wstring strNextFile = *cit;
			wstring strFullPath = strPanelDir + strNextFile;

			std::string hashValueStr;
			bool fShouldAbort = false;

			if (RunGeneration(strFullPath, strNextFile, genAlgo, optHashUppercase != FALSE, progressCtx, hashValueStr, fShouldAbort, fAutoSkipErrors))
			{
				hashes.SetFileHash(storeAbsPaths ? strFullPath : strNextFile, hashValueStr, genAlgo);
			}
			else if (fShouldAbort)
			{
				continueSave = false;
				break;
			}
		}

		FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_SETPROGRESSSTATE, (void*)PS_NOPROGRESS);
		FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_PROGRESSNOTIFY, 0);

		if (!continueSave) return;
	}

	// Display/save hash list
	bool saveSuccess = false;
	if (outputTarget == OT_SINGLEFILE)
	{
		saveSuccess = hashes.SaveList(outputFile.c_str(), outputFileCodepage);
		if (!saveSuccess)
		{
			DisplayMessage(MSG_DLG_ERROR, MSG_DLG_CANT_SAVE_HASHLIST, outputFile.c_str(), true, true);
		}
	}
	else if (outputTarget == OT_SEPARATEFILES)
	{
		int numGood, numBad;
		saveSuccess = hashes.SaveListSeparate(strPanelDir.c_str(), outputFileCodepage, numGood, numBad);
	}
	else if (outputTarget == OT_DISPLAY)
	{
		saveSuccess = true;
		DisplayHashListOnScreen(hashes);
	}
	else
	{
		// Something went wrong. This should not happen.
		DisplayMessage(L"Error", L"Invalid output target", nullptr, true, true);
	}

	// Clear selection if requested
	if (saveSuccess && optClearSelectionOnComplete)
	{
		for (int i = pi.SelectedItemsNumber - 1; i >=0; i--)
			FarSInfo.Control(PANEL_ACTIVE, FCTL_CLEARSELECTION, i, NULL);
	}

	if (optRememberLastUsedAlgo)
	{
		optDefaultAlgo = genAlgo;
		SaveSettings();
	}

	FarSInfo.Control(PANEL_ACTIVE, FCTL_REDRAWPANEL, 0, NULL);
}

static bool AskForCompareParams(rhash_ids &selectedAlgo, bool &recursive)
{
	FarDialogItem DialogItems []={
		/*0*/{DI_DOUBLEBOX,		3, 1, 41,14, 0, 0, 0, 0, GetLocMsg(MSG_DLG_COMPARE)},

		/*1*/{DI_TEXT,			5, 2, 0, 0, 0, 0, 0, 0, GetLocMsg(MSG_GEN_ALGO), 0},
		/*2*/{ DI_RADIOBUTTON, 6, 3, 0, 0, 0, (selectedAlgo == RHASH_CRC32), DIF_GROUP, 0, GetLocMsg(MSG_ALGO_CRC) },
		/*3*/{ DI_RADIOBUTTON, 6, 4, 0, 0, 0, (selectedAlgo == RHASH_MD5), 0, 0, GetLocMsg(MSG_ALGO_MD5) },
		/*4*/{ DI_RADIOBUTTON, 6, 5, 0, 0, 0, (selectedAlgo == RHASH_SHA1), 0, 0, GetLocMsg(MSG_ALGO_SHA1) },
		/*5*/{ DI_RADIOBUTTON, 6, 6, 0, 0, 0, (selectedAlgo == RHASH_SHA256), 0, 0, GetLocMsg(MSG_ALGO_SHA256) },
		/*6*/{ DI_RADIOBUTTON, 6, 7, 0, 0, 0, (selectedAlgo == RHASH_SHA512), 0, 0, GetLocMsg(MSG_ALGO_SHA512) },
		/*7*/{ DI_RADIOBUTTON, 6, 8, 0, 0, 0, (selectedAlgo == RHASH_SHA3_512), 0, 0, GetLocMsg(MSG_ALGO_SHA3_512) },
		/*8*/{ DI_RADIOBUTTON, 6, 9, 0, 0, 0, (selectedAlgo == RHASH_WHIRLPOOL), 0, 0, GetLocMsg(MSG_ALGO_WHIRLPOOL) },

		/*9*/{DI_TEXT,			3,10, 0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L""},
		/*10*/{DI_CHECKBOX,		5,11, 0, 0, 0, recursive, 0, 0, GetLocMsg(MSG_GEN_RECURSE)},

		/*11*/{DI_TEXT,			3,12, 0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L"", 0},
		/*12*/{DI_BUTTON,		0,13, 0,13, 0, 0, DIF_CENTERGROUP, 1, GetLocMsg(MSG_BTN_RUN), 0},
		/*13*/{DI_BUTTON,		0,13, 0,13, 0, 0, DIF_CENTERGROUP, 0, GetLocMsg(MSG_BTN_CANCEL), 0},
	};
	size_t numDialogItems = sizeof(DialogItems) / sizeof(DialogItems[0]);

	HANDLE hDlg = FarSInfo.DialogInit(FarSInfo.ModuleNumber, -1, -1, 45, 16, L"CompareParams", DialogItems, (unsigned) numDialogItems, 0, 0, FarSInfo.DefDlgProc, 0);

	bool retVal = false;
	if (hDlg != INVALID_HANDLE_VALUE)
	{
		int ExitCode = FarSInfo.DialogRun(hDlg);
		if (ExitCode == numDialogItems - 2) // OK was pressed
		{
			recursive = DlgHlp_GetSelectionState(hDlg, 10) != 0;

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

	StringList vSelectedFiles;
	int64_t totalFilesSize = 0;

	FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_SETPROGRESSSTATE, (void*) PS_INDETERMINATE);
		
	// Prepare files list
	{
		FarScreenSave screen;
		DisplayMessage(GetLocMsg(MSG_DLG_PROCESSING), GetLocMsg(MSG_DLG_PREPARE_LIST), NULL, false, false);

		GetSelectedPanelFiles(piActv, strActivePanelDir, vSelectedFiles, totalFilesSize, true);
	}

	// No suitable items selected for comparison
	if (vSelectedFiles.size() == 0) return;

	std::vector<wstring> vMismatches, vMissing;
	int nFilesSkipped = 0;
	bool fAborted = false;
	bool fSkipAllErrors = false;

	{
		ProgressContext progressCtx((int)vSelectedFiles.size() * 2, totalFilesSize * 2);

		for (auto cit = vSelectedFiles.begin(); cit != vSelectedFiles.end(); cit++)
		{
			wstring strNextFile = *cit;

			wstring strActvPath = strActivePanelDir + strNextFile;
			wstring strPasvPath = strPassivePanelDir + strNextFile;

			int64_t nActivePanelFileSize = GetFileSize_i64(strActvPath.c_str());
			int64_t nPassivePanelFileSize;

			// Does opposite file exists at all?
			if (!IsFile(strPasvPath, &nPassivePanelFileSize))
			{
				vMissing.push_back(strNextFile);
				progressCtx.CurrentFileIndex += 2;
				progressCtx.TotalProcessedBytes += nActivePanelFileSize * 2;
				continue;
			}

			// For speed compare file sizes first
			if (nActivePanelFileSize != nPassivePanelFileSize)
			{
				vMismatches.push_back(strNextFile);
				progressCtx.CurrentFileIndex += 2;
				progressCtx.TotalProcessedBytes += nActivePanelFileSize * 2;
				continue;
			}

			std::string strHashValueActive;
			std::string strHashValuePassive;

			if (RunGeneration(strActvPath, strActvPath, cmpAlgo, false, progressCtx, strHashValueActive, fAborted, fSkipAllErrors)
				&& RunGeneration(strPasvPath, strPasvPath, cmpAlgo, false, progressCtx, strHashValuePassive, fAborted, fSkipAllErrors))
			{
				if (!SameHash(strHashValueActive, strHashValuePassive))
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

		FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_SETPROGRESSSTATE, (void*)PS_NOPROGRESS);
		FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_PROGRESSNOTIFY, 0);
	}

	if (!fAborted)
	{
		DisplayValidationResults(vMismatches, vMissing, nFilesSkipped);
	}

	if (optRememberLastUsedAlgo)
	{
		optDefaultAlgo = cmpAlgo;
		SaveSettings();
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
		intptr_t selItem = algoMenu.Run();
		if (selItem < 0) return;

		selectedAlgoIndex = algoIndicies[selItem];
	}

	rhash_ids algo = SupportedHashes[selectedAlgoIndex].AlgoId;
	std::string strHashValue;
	bool fAborted = false, fSkipAllErrors = false;

	ProgressContext progressCtx(1, GetFileSize_i64(selectedFile.c_str()));

	if (RunGeneration(selectedFile, selectedFile, algo, false, progressCtx, strHashValue, fAborted, fSkipAllErrors))
	{
		if (SameHash(strHashValue, clipText))
			DisplayMessage(GetLocMsg(MSG_DLG_CALC_COMPLETE), GetLocMsg(MSG_DLG_FILE_CLIP_MATCH), NULL, false, true);
		else
			DisplayMessage(GetLocMsg(MSG_DLG_CALC_COMPLETE), GetLocMsg(MSG_DLG_FILE_CLIP_MISMATCH), NULL, true, true);
	}

	FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_SETPROGRESSSTATE, (void*) PS_NOPROGRESS);
	FarSInfo.AdvControl(FarSInfo.ModuleNumber, ACTL_PROGRESSNOTIFY, 0);
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

	const wchar_t* codePageNames[] = {L"UTF-8", L"ANSI", L"OEM"};
	const UINT codePageValues[] = {CP_UTF8, CP_ACP, CP_OEMCP};

	FarListItem cpListItems[ARRAY_SIZE(codePageNames)] = {0};
	FarList cpDlgList = {ARRAY_SIZE(codePageNames), cpListItems};
	
	FarDialogItem DialogItems []={
		/*00*/ {DI_DOUBLEBOX, 3, 1,40,15, 0, 0, 0,0, GetLocMsg(MSG_CONFIG_TITLE), 0},
		/*01*/ {DI_TEXT,	  5, 2, 0, 0, 0, 0, 0, 0, GetLocMsg(MSG_CONFIG_DEFAULT_ALGO), 0},
		/*02*/ {DI_COMBOBOX,  5, 3,20, 0, 0, (DWORD_PTR)&algoDlgList, DIF_DROPDOWNLIST, 0, NULL, 0},
		/*03*/ {DI_CHECKBOX,  5, 4, 0, 0, 0, (DWORD_PTR)optRememberLastUsedAlgo, 0,0, GetLocMsg(MSG_CONFIG_REMEMBER_LAST_ALGO), 0},
		/*04*/ {DI_TEXT,	  3, 5, 0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L"", 0},
		/*05*/ {DI_CHECKBOX,  5, 6, 0, 0, 0, (DWORD_PTR)optUsePrefix, 0,0, GetLocMsg(MSG_CONFIG_PREFIX), 0},
		/*06*/ {DI_EDIT,	  8, 7,24, 0, 0, 0, 0,0, optPrefix, 0},
		/*07*/ {DI_CHECKBOX,  5, 8, 0, 0, 0, (DWORD_PTR)optConfirmAbort, 0,0, GetLocMsg(MSG_CONFIG_CONFIRM_ABORT), 0},
		/*08*/ {DI_CHECKBOX,  5, 9, 0, 0, 0, (DWORD_PTR)optClearSelectionOnComplete, 0,0, GetLocMsg(MSG_CONFIG_CLEAR_SELECTION), 0},
		/*09*/ {DI_CHECKBOX,  5,10, 0, 0, 0, (DWORD_PTR)optAutoExtension, 0,0, GetLocMsg(MSG_CONFIG_AUTOEXT), 0},
		/*10*/ {DI_CHECKBOX,  5,11, 0, 0, 0, (DWORD_PTR)optHashUppercase, 0,0, GetLocMsg(MSG_CONFIG_UPPERCASE), 0},
		/*11*/ {DI_TEXT,	  5,12, 0, 0, 0, 0, 0, 0, GetLocMsg(MSG_CONFIG_DEFAULT_CP), 0},
		/*12*/ {DI_COMBOBOX,  5,12, 0, 0, 0, (DWORD_PTR)&cpDlgList, DIF_DROPDOWNLIST, 0, NULL, 0},
		/*13*/ {DI_TEXT,	  3,13, 0, 0, 0, 0, DIF_BOXCOLOR|DIF_SEPARATOR, 0, L"", 0},
		/*14*/ {DI_BUTTON,	  0,14, 0, 0, 0, 0, DIF_CENTERGROUP, 1, GetLocMsg(MSG_BTN_OK), 0},
		/*15*/ {DI_BUTTON,    0,14, 0, 0, 1, 0, DIF_CENTERGROUP, 0, GetLocMsg(MSG_BTN_CANCEL), 0},
	};
	int DialogItemsCount = ARRAY_SIZE(DialogItems);

	for (int i = 0; i < NUMBER_OF_SUPPORTED_HASHES; i++)
	{
		algoListItems[i].Text = SupportedHashes[i].AlgoName.c_str();
		if (SupportedHashes[i].AlgoId == optDefaultAlgo)
			algoListItems[i].Flags = LIF_SELECTED;
	}

	for (int i = 0; i < ARRAY_SIZE(codePageValues); i++)
	{
		cpListItems[i].Text = codePageNames[i];
		if (codePageValues[i] == optListDefaultCodepage)
			cpListItems[i].Flags = LIF_SELECTED;
	}

	// Set proper location for codepage combobox
	DialogItems[12].X1 += (int) wcslen(DialogItems[11].PtrData) + 1;
	DialogItems[12].X2 += DialogItems[12].X1 + 6;

	// Expand right border of the dialog if test is too long to fit
	int borderX2 = AdjustDialogBorder(DialogItems, DialogItemsCount);

	HANDLE hDlg = FarSInfo.DialogInit(FarSInfo.ModuleNumber, -1, -1, borderX2 + 4, DialogItemsCount + 1, L"IntCheckerConfig", DialogItems, DialogItemsCount, 0, 0, FarSInfo.DefDlgProc, 0);

	int nOkID = DialogItemsCount - 2;

	if (hDlg != INVALID_HANDLE_VALUE)
	{
		int ExitCode = FarSInfo.DialogRun(hDlg);
		if (ExitCode == nOkID) // OK was pressed
		{
			optRememberLastUsedAlgo = DlgHlp_GetSelectionState(hDlg, 4);
			optUsePrefix = DlgHlp_GetSelectionState(hDlg, 5);
			DlgHlp_GetEditBoxText(hDlg, 6, optPrefix, ARRAY_SIZE(optPrefix));
			optConfirmAbort = DlgHlp_GetSelectionState(hDlg, 7);
			optClearSelectionOnComplete = DlgHlp_GetSelectionState(hDlg, 8);
			optAutoExtension = DlgHlp_GetSelectionState(hDlg, 9);
			optHashUppercase = DlgHlp_GetSelectionState(hDlg, 10);
			
			int selectedAlgo = (int) DlgList_GetCurPos(FarSInfo, hDlg, 2);
			optDefaultAlgo = SupportedHashes[selectedAlgo].AlgoId;

			int selectedCodepage = (int) DlgList_GetCurPos(FarSInfo, hDlg, 12);
			optListDefaultCodepage = codePageValues[selectedCodepage];

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
			if (!RunValidateFiles((wchar_t*) Item, true, false))
				DisplayMessage(GetLocMsg(MSG_DLG_ERROR), GetLocMsg(MSG_DLG_NOTVALIDLIST), NULL, true, true);
		}
	}
	else if (OpenFrom == OPEN_PLUGINSMENU)
	{
		// We are from regular plug-ins menu

		PanelInfo pi = {0};
		if (!FarSInfo.Control(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, (LONG_PTR)&pi) || (pi.PanelType != PTYPE_FILEPANEL) || ((pi.Plugin != 0) && !(pi.Flags & PFLAGS_REALNAMES)))
		{
			return INVALID_HANDLE_VALUE;
		}

		FarMenu openMenu(&FarSInfo, GetLocMsg(MSG_PLUGIN_NAME));

		openMenu.AddItemEx(GetLocMsg(MSG_MENU_GENERATE), boost::bind(RunGenerateHashes));
		openMenu.AddItemEx(GetLocMsg(MSG_MENU_COMPARE), boost::bind(RunComparePanels));

		wstring selectedFilePath;
		if ((pi.SelectedItemsNumber == 1) && GetSelectedPanelItemPath(selectedFilePath) && IsFile(selectedFilePath))
		{
			//TODO: use optDetectHashFiles
			openMenu.AddItemEx(GetLocMsg(MSG_MENU_VALIDATE), boost::bind(RunValidateFiles, selectedFilePath.c_str(), false, false));
			openMenu.AddItemEx(GetLocMsg(MSG_MENU_VALIDATE_WITH_PARAMS), boost::bind(RunValidateFiles, selectedFilePath.c_str(), false, true));
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
