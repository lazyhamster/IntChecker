#include "stdafx.h"

#include <far3/plugin.hpp>
#include <far3/DlgBuilder.hpp>
#include <far3/PluginSettings.hpp>

#include "version.h"
#include "Utils.h"

#include <InitGuid.h>
#include "Far3Guids.h"

#include "FarCommon.h"

// --------------------------------------- Service functions -------------------------------------------------

static intptr_t FarAdvControl(enum ADVANCED_CONTROL_COMMANDS command, intptr_t param1 = 0, void* param2 = NULL)
{
	return FarSInfo.AdvControl(&GUID_PLUGIN_MAIN, command, param1, param2);
}

static const wchar_t* GetLocMsg(int MsgID)
{
	return FarSInfo.GetMsg(&GUID_PLUGIN_MAIN, MsgID);
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

	FarSInfo.Message(&GUID_PLUGIN_MAIN, &GUID_MESSAGE_BOX, flags, NULL, MsgLines, linesNum, 0);
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

	intptr_t resp = FarSInfo.Message(&GUID_PLUGIN_MAIN, &GUID_MESSAGE_BOX, flags, NULL, MsgLines, 2, 0);
	return (resp == 0);
}

static bool ConfirmMessage(int headerMsgID, int textMsgID, bool isWarning)
{
	return ConfirmMessage(GetLocMsg(headerMsgID), GetLocMsg(textMsgID), isWarning);
}

static bool GetPanelDir(HANDLE hPanel, wstring& dirStr)
{
	bool ret = false;

	size_t nBufSize = FarSInfo.PanelControl(hPanel, FCTL_GETPANELDIRECTORY, 0, NULL);
	FarPanelDirectory *panelDir = (FarPanelDirectory*) malloc(nBufSize);
	panelDir->StructSize = sizeof(FarPanelDirectory);
	if (FarSInfo.PanelControl(hPanel, FCTL_GETPANELDIRECTORY, nBufSize, panelDir))
	{
		dirStr.assign(panelDir->Name);
		IncludeTrailingPathDelim(dirStr);
		ret = true;
	}

	free(panelDir);
	return ret;
}

static bool GetSelectedPanelFilePath(wstring& nameStr)
{
	nameStr.clear();

	PanelInfo pi = {sizeof(PanelInfo)};
	if (FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, &pi))
		if ((pi.SelectedItemsNumber == 1) && (pi.PanelType == PTYPE_FILEPANEL))
		{
			intptr_t dirBufSize = FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_GETPANELDIRECTORY, 0, NULL);
			FarPanelDirectory *panelDir = (FarPanelDirectory*) malloc(dirBufSize);
			panelDir->StructSize = sizeof(FarPanelDirectory);
			FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_GETPANELDIRECTORY, dirBufSize, panelDir);

			wstring strNameBuffer = panelDir->Name;
			IncludeTrailingPathDelim(strNameBuffer);

			size_t itemBufSize = FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_GETCURRENTPANELITEM, 0, NULL);
			PluginPanelItem *PPI = (PluginPanelItem*)malloc(itemBufSize);
			if (PPI)
			{
				FarGetPluginPanelItem FGPPI={sizeof(FarGetPluginPanelItem), itemBufSize, PPI};
				FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_GETCURRENTPANELITEM, 0, &FGPPI);
				if ((PPI->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
				{
					strNameBuffer.append(PPI->FileName);
					nameStr = strNameBuffer;
				}
				free(PPI);
			}
		}

		return (nameStr.size() > 0);
}

static void GetSelectedPanelFiles(PanelInfo &pi, wstring &panelDir, StringList &vDest, int64_t &totalSize, bool recursive)
{
	for (size_t i = 0; i < pi.SelectedItemsNumber; i++)
	{
		size_t requiredBytes = FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_GETSELECTEDPANELITEM, i, NULL);
		PluginPanelItem *PPI = (PluginPanelItem*)malloc(requiredBytes);
		if (PPI)
		{
			FarGetPluginPanelItem FGPPI = {sizeof(FarGetPluginPanelItem), requiredBytes, PPI};
			FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_GETSELECTEDPANELITEM, i, &FGPPI);
			if (wcscmp(PPI->FileName, L"..") && wcscmp(PPI->FileName, L"."))
			{
				if ((PPI->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
				{
					vDest.push_back(PPI->FileName);
					totalSize += PPI->FileSize;
				}
				else
				{
					wstring strSelectedDir = panelDir + PPI->FileName;
					PrepareFilesList(strSelectedDir.c_str(), PPI->FileName, vDest, totalSize, recursive);
				}
			}
			free(PPI);
		}
	}
}

// --------------------------------------- Local functions ---------------------------------------------------

static void LoadSettings()
{
	PluginSettings ps(GUID_PLUGIN_MAIN, FarSInfo.SettingsControl);
	
	optDetectHashFiles			= ps.Get(0, L"DetectHashFiles", optDetectHashFiles);
	optClearSelectionOnComplete = ps.Get(0, L"ClearSelection", optClearSelectionOnComplete);
	optConfirmAbort				= ps.Get(0, L"ConfirmAbort", optConfirmAbort);
	optDefaultAlgo				= ps.Get(0, L"DefaultHash", optDefaultAlgo);
	optUsePrefix				= ps.Get(0, L"UsePrefix", optUsePrefix);
	optAutoExtension			= ps.Get(0, L"AutoExtension", optAutoExtension);

	ps.Get(0, L"Prefix", optPrefix, ARRAY_SIZE(optPrefix));
}

static void SaveSettings()
{
	PluginSettings ps(GUID_PLUGIN_MAIN, FarSInfo.SettingsControl);
	
	ps.Set(0, L"DetectHashFiles", optDetectHashFiles);
	ps.Set(0, L"ClearSelection", optClearSelectionOnComplete);
	ps.Set(0, L"ConfirmAbort", optConfirmAbort);
	ps.Set(0, L"DefaultHash", optDefaultAlgo);
	ps.Set(0, L"Prefix", optPrefix);
	ps.Set(0, L"UsePrefix", optUsePrefix);
	ps.Set(0, L"AutoExtension", optAutoExtension);
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

		FarSInfo.Message(&GUID_PLUGIN_MAIN, &GUID_MESSAGE_BOX, 0, NULL, InfoLines, ARRAY_SIZE(InfoLines), 0);

		// Win7 only feature
		if (prCtx->TotalFilesSize > 0)
		{
			ProgressValue pv;
			pv.StructSize = sizeof(ProgressValue);
			pv.Completed = prCtx->TotalProcessedBytes;
			pv.Total = prCtx->TotalFilesSize;
			FarAdvControl(ACTL_SETPROGRESSVALUE, 0, &pv);
		}
	}

	return true;
}

static void SelectFilesOnPanel(HANDLE hPanel, vector<wstring> &fileNames, bool isSelected)
{
	if (fileNames.size() == 0) return;

	PanelInfo pi = {0};
	FarSInfo.PanelControl(hPanel, FCTL_GETPANELINFO, 0, &pi);

	FarSInfo.PanelControl(hPanel, FCTL_BEGINSELECTION, 0, NULL);

	for (size_t i = 0; i < pi.ItemsNumber; i++)
	{
		size_t nSize = FarSInfo.PanelControl(hPanel, FCTL_GETPANELITEM, i, NULL);
		PluginPanelItem *PPI = (PluginPanelItem*) malloc(nSize);
		if (PPI)
		{
			FarGetPluginPanelItem gppi = {sizeof(FarGetPluginPanelItem), nSize, PPI};
			FarSInfo.PanelControl(hPanel, FCTL_GETPANELITEM, i, &gppi);
			if (std::find(fileNames.begin(), fileNames.end(), PPI->FileName) != fileNames.end())
			{
				FarSInfo.PanelControl(hPanel, FCTL_SETSELECTION, i, (void*) (isSelected ? TRUE : FALSE));
			}
			free(PPI);
		}
	}

	FarSInfo.PanelControl(hPanel, FCTL_ENDSELECTION, 0, NULL);
	FarSInfo.PanelControl(hPanel, FCTL_REDRAWPANEL, 0, NULL);
}

static void DisplayValidationResults(std::vector<std::wstring> &vMismatchList, std::vector<std::wstring> &vMissingList, int numSkipped)
{
	if (vMismatchList.size() == 0 && vMissingList.size() == 0)
	{
		// If everything is fine then just display simple message
		static wchar_t wszGoodMessage[256];
		if (numSkipped == 0)
			wcscpy_s(wszGoodMessage, ARRAY_SIZE(wszGoodMessage), L"No mismatches found");
		else
			swprintf_s(wszGoodMessage, ARRAY_SIZE(wszGoodMessage), L"No mismatches found (%d file(s) were skipped)", numSkipped);

		DisplayMessage(GetLocMsg(MSG_DLG_VALIDATION_COMPLETE), wszGoodMessage, NULL, false, true);
	}
	else
	{
		// Otherwise display proper list of invalid/missing files

		//Prepare list
		vector<wstring> displayStrings;

		size_t nListIndex = 0;
		if (vMismatchList.size() > 0)
		{
			displayStrings.push_back(FormatString(GetLocMsg(MSG_DLG_MISMATCHED_FILES), vMismatchList.size()));

			for (size_t i = 0; i < vMismatchList.size(); i++)
			{
				wstring &nextFile = vMismatchList[i];
				displayStrings.push_back(FormatString(L"\t\t%s", nextFile.c_str()));
			}
		}
		if (vMissingList.size() > 0)
		{
			displayStrings.push_back(FormatString(GetLocMsg(MSG_DLG_MISSING_FILES), vMissingList.size()));

			for (size_t i = 0; i < vMissingList.size(); i++)
			{
				wstring &nextFile = vMissingList[i];
				displayStrings.push_back(FormatString(L"\t\t%s", nextFile.c_str()));
			}
		}

		// Display dialog

		PluginDialogBuilder dlgBuilder(FarSInfo, GUID_PLUGIN_MAIN, GUID_DIALOG_RESULTS, MSG_DLG_VALIDATION_COMPLETE, nullptr);

		const wchar_t* *boxList = new const wchar_t*[displayStrings.size()];
		for (size_t i = 0; i < displayStrings.size(); i++)
		{
			boxList[i] = displayStrings[i].c_str();
		}

		dlgBuilder.AddListBox(nullptr, 57, 14, boxList, displayStrings.size(), DIF_LISTNOBOX | DIF_LISTNOCLOSE);
		dlgBuilder.AddOKCancel(MSG_BTN_CLOSE, -1, -1, true);

		dlgBuilder.ShowDialog();

		delete [] boxList;

		// Select mismatched files that are in the same folder
		
		vector<wstring> vSameFolderFiles;
		for (size_t i = 0; i < vMismatchList.size(); i++)
		{
			wstring &nextFile = vMismatchList[i];
			if (nextFile.find_first_of(L"\\/") == wstring::npos)
				vSameFolderFiles.push_back(nextFile);
		}
		
		SelectFilesOnPanel(PANEL_ACTIVE, vSameFolderFiles, true);
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
	FarAdvControl(ACTL_SETPROGRESSSTATE, TBPS_INDETERMINATE, NULL);

	// Prepare files list
	{
		FarScreenSave screen;
		DisplayMessage(GetLocMsg(MSG_DLG_PROCESSING), GetLocMsg(MSG_DLG_PREPARE_LIST), NULL, false, false);

		for (size_t i = 0; i < hashes.GetCount(); i++)
		{
			FileHashInfo fileInfo = hashes.GetFileInfo(i);

			wstring strFullFilePath = workDir + fileInfo.Filename;
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
			wstring strFullFilePath = workDir + fileInfo.Filename;

			progressCtx.FileName = fileInfo.Filename;
			progressCtx.CurrentFileIndex++;
			progressCtx.CurrentFileProcessedBytes = 0;
			progressCtx.CurrentFileSize = GetFileSize_i64(strFullFilePath.c_str());
			progressCtx.FileProgress = 0;

			{
				FarScreenSave screen;
				int genRetVal = GenerateHash(strFullFilePath.c_str(), hashes.GetHashAlgo(), hashValueBuf, FileHashingProgress, &progressCtx);

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

	FarAdvControl(ACTL_SETPROGRESSSTATE, TBPS_NOPROGRESS, NULL);
	FarAdvControl(ACTL_PROGRESSNOTIFY, 0, NULL);

	return true;
}

static intptr_t WINAPI HashParamsDlgProc(HANDLE hDlg, intptr_t Msg, intptr_t Param1, void* Param2)
{
	const int nTextBoxIndex = 13;

	if (Msg == DN_BTNCLICK && optAutoExtension)
	{
		if (Param2 && (Param1 >= 2) && (Param1 <= 2 + NUMBER_OF_SUPPORTED_HASHES))
		{
			int selectedHashIndex = (int) Param1 - 2;
			wchar_t wszHashFileName[MAX_PATH];

			FarDialogItemData fdid = {sizeof(FarDialogItemData), 0, wszHashFileName};
			FarSInfo.SendDlgMessage(hDlg, DM_GETTEXT, nTextBoxIndex, &fdid);

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
						FarSInfo.SendDlgMessage(hDlg, DM_SETTEXTPTR, nTextBoxIndex, wszHashFileName);
						break;
					}
				}
			}

			return TRUE;
		}
	}

	return FarSInfo.DefDlgProc(hDlg, Msg, Param1, Param2);
}

static bool AskForHashGenerationParams(rhash_ids &selectedAlgo, bool &recursive, HashOutputTargets &outputTarget, wstring &outputFileName)
{
	int algoIndex = GetAlgoIndex(selectedAlgo);
	int algoNames[] = {MSG_ALGO_CRC, MSG_ALGO_MD5, MSG_ALGO_SHA1, MSG_ALGO_SHA256, MSG_ALGO_SHA512, MSG_ALGO_WHIRLPOOL};
	int targetIndex = outputTarget;
	int doRecurse = recursive;
	
	wchar_t outputFileBuf[MAX_PATH] = {0};
	wcscpy_s(outputFileBuf, ARRAY_SIZE(outputFileBuf), outputFileName.c_str());
		
	PluginDialogBuilder dlgBuilder(FarSInfo, GUID_PLUGIN_MAIN, GUID_DIALOG_PARAMS, MSG_GEN_TITLE, L"GenerateParams", HashParamsDlgProc);

	dlgBuilder.AddText(MSG_GEN_ALGO);
	dlgBuilder.AddRadioButtons(&algoIndex, ARRAY_SIZE(algoNames), algoNames);
	dlgBuilder.AddSeparator();
	dlgBuilder.AddText(MSG_GEN_TARGET);
	
	dlgBuilder.AddRadioButton(&targetIndex, MSG_GEN_TO_FILE, true, true);
	dlgBuilder.AddRadioButton(&targetIndex, MSG_GEN_TO_SEPARATE, false, false)->Y1++;
	dlgBuilder.AddRadioButton(&targetIndex, MSG_GEN_TO_SCREEN, false, false)->Y1++;
	
	auto editFileName = dlgBuilder.AddEditField(outputFileBuf, MAX_PATH, 30);
	editFileName->Flags = DIF_EDITEXPAND|DIF_EDITPATH;
	editFileName->X1 += 2;
	editFileName->Y1 -= 2;
	
	dlgBuilder.AddSeparator();
	dlgBuilder.AddCheckbox(MSG_GEN_RECURSE, &doRecurse, 0, false);
	dlgBuilder.AddOKCancel(MSG_BTN_RUN, MSG_BTN_CANCEL, -1, true);

	if (dlgBuilder.ShowDialog())
	{
		recursive = doRecurse != 0;
		selectedAlgo = SupportedHashes[algoIndex].AlgoId;
		outputTarget = (HashOutputTargets) targetIndex;
		outputFileName = outputFileBuf;
		
		return true;
	}
		
	return false;
}

static void DisplayHashListOnScreen(HashList &list)
{
	vector<wstring> listStrDump;
	LPCWSTR *listBoxItems = new LPCWSTR[list.GetCount()];

	for (size_t i = 0; i < list.GetCount(); i++)
	{
		listStrDump.push_back(list.FileInfoToString(i));

		wstring &line = listStrDump[i];
		listBoxItems[i] = line.c_str();
	}
	
	PluginDialogBuilder dlgBuilder(FarSInfo, GUID_PLUGIN_MAIN, GUID_DIALOG_RESULTS, MSG_GEN_TITLE, nullptr);

	dlgBuilder.AddListBox(NULL, 60, 15, listBoxItems, list.GetCount(), DIF_LISTNOCLOSE | DIF_LISTNOBOX);
	dlgBuilder.AddOKCancel(MSG_BTN_CLOSE, MSG_BTN_CLIPBOARD, -1, true);
	
	intptr_t exitCode = dlgBuilder.ShowDialogEx();
	if (exitCode == 1)
	{
		CopyTextToClipboard(listStrDump);
	}

	delete [] listBoxItems;
}

static int DisplayHashGenerateError(const wstring& fileName)
{
	static const wchar_t* DlgLines[6];
	DlgLines[0] = GetLocMsg(MSG_DLG_ERROR);
	DlgLines[1] = L"Can not calculate hash for";
	DlgLines[2] = fileName.c_str();
	DlgLines[3] = L"Skip";
	DlgLines[4] = L"Retry";
	DlgLines[5] = GetLocMsg(MSG_BTN_CANCEL);

	return (int) FarSInfo.Message(&GUID_PLUGIN_MAIN, &GUID_MESSAGE_BOX, FMSG_WARNING, NULL, DlgLines, ARRAY_SIZE(DlgLines), 3);
}

static void RunGenerateHashes()
{
	// Check panel for compatibility
	PanelInfo pi = {sizeof(PanelInfo), 0};
	if (!FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, &pi)
		|| (pi.SelectedItemsNumber <= 0) || (pi.PanelType != PTYPE_FILEPANEL))
	{
		DisplayMessage(GetLocMsg(MSG_DLG_ERROR), GetLocMsg(MSG_DLG_INVALID_PANEL), NULL, true, true);
		return;
	}

	// Generation params
	rhash_ids genAlgo = (rhash_ids) optDefaultAlgo;
	bool recursive = true;
	HashOutputTargets outputTarget = OT_SINGLEFILE;
	wstring outputFile(L"hashlist");

	HashAlgoInfo *selectedHashInfo = GetAlgoInfo(genAlgo);
	if (!selectedHashInfo) return;

	if (optAutoExtension) outputFile += selectedHashInfo->DefaultExt;

	// If only one file is selected then offer it's name as base for hash file name
	if (pi.SelectedItemsNumber == 1)
	{
		wstring strSelectedFile;
		if (GetSelectedPanelFilePath(strSelectedFile) && IsFile(strSelectedFile.c_str()))
		{
			outputFile = ExtractFileName(strSelectedFile) + selectedHashInfo->DefaultExt;
		}
	}

	while(true)
	{
		if (!AskForHashGenerationParams(genAlgo, recursive, outputTarget, outputFile))
			return;

		wchar_t fullPath[PATH_BUFFER_SIZE];
		FSF.ConvertPath(CPM_FULL, outputFile.c_str(), fullPath, ARRAY_SIZE(fullPath));
		outputFile = fullPath;

		// Check if hash file already exists
		if ((outputTarget == OT_SINGLEFILE) && IsFile(outputFile.c_str()))
		{
			wchar_t wszMsgText[256] = {0};
			swprintf_s(wszMsgText, ARRAY_SIZE(wszMsgText), GetLocMsg(MSG_DLG_OVERWRITE_FILE_TEXT), outputFile.c_str());

			if (!ConfirmMessage(GetLocMsg(MSG_DLG_OVERWRITE_FILE), wszMsgText, true))
				continue;
		}

		break;
	}

	StringList filesToProcess;
	int64_t totalFilesSize = 0;
	HashList hashes(genAlgo);
	wstring strPanelDir;

	// Win7 only feature
	FarAdvControl(ACTL_SETPROGRESSSTATE, TBPS_INDETERMINATE, NULL);

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

				int genRetVal = GenerateHash(strFullPath.c_str(), genAlgo, hashValueBuf, FileHashingProgress, &progressCtx);

				if (genRetVal == GENERATE_ABORTED)
				{
					// Exit silently
					continueSave = false;
				}
				else if (genRetVal == GENERATE_ERROR)
				{
					int resp = DisplayHashGenerateError(strNextFile);
					if (resp == EDR_RETRY)
						continue;
					else if (resp == EDR_SKIP)
						fSaveHash = false;
					else
						continueSave = false;
				}

				// Always break if not said otherwise
				break;
			}
		}

		if (!continueSave) break;

		if (fSaveHash)
			hashes.SetFileHash(strNextFile.c_str(), hashValueBuf);
	}

	FarAdvControl(ACTL_SETPROGRESSSTATE, TBPS_NOPROGRESS, NULL);
	FarAdvControl(ACTL_PROGRESSNOTIFY, 0, NULL);

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
		for (size_t i = pi.SelectedItemsNumber - 1; i >=0; i--)
			FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_CLEARSELECTION, i, NULL);
	}

	FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_REDRAWPANEL, 0, NULL);
}

static bool AskForCompareParams(rhash_ids &selectedAlgo, bool &recursive)
{
	int doRecurse = recursive;
	int algoIndex = GetAlgoIndex(selectedAlgo);
	int algoNames[] = {MSG_ALGO_CRC, MSG_ALGO_MD5, MSG_ALGO_SHA1, MSG_ALGO_SHA256, MSG_ALGO_SHA512, MSG_ALGO_WHIRLPOOL};
	
	PluginDialogBuilder dlgBuilder(FarSInfo, GUID_PLUGIN_MAIN, GUID_DIALOG_PARAMS, MSG_DLG_COMPARE, NULL);

	dlgBuilder.AddText(MSG_GEN_ALGO);
	dlgBuilder.AddRadioButtons(&algoIndex, ARRAY_SIZE(algoNames), algoNames, false);
	dlgBuilder.AddSeparator();
	dlgBuilder.AddCheckbox(MSG_GEN_RECURSE, &doRecurse, 0, false);
	dlgBuilder.AddOKCancel(MSG_BTN_RUN, MSG_BTN_CANCEL, -1, true);

	if (dlgBuilder.ShowDialog())
	{
		recursive = doRecurse != 0;
		selectedAlgo = SupportedHashes[algoIndex].AlgoId;
		
		return true;
	}

	return false;
}

static bool RunGeneration(const wstring& filePath, rhash_ids hashAlgo, ProgressContext& progressCtx, char* hashStrBuffer, bool &shouldAbort)
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
		int genRetVal = GenerateHash(filePath.c_str(), hashAlgo, hashStrBuffer, FileHashingProgress, &progressCtx);

		if (genRetVal == GENERATE_ABORTED)
		{
			// Exit silently
			shouldAbort = true;
			return false;
		}
		else if (genRetVal == GENERATE_ERROR)
		{
			int errResp = DisplayHashGenerateError(filePath);
			if (errResp == EDR_RETRY)
				continue;
			else
				shouldAbort = (errResp == EDR_ABORT);

			return false;
		}

		break;
	}

	return true;
}

static void RunComparePanels()
{
	PanelInfo piActv, piPasv;
	if (!FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, &piActv)
		|| !FarSInfo.PanelControl(PANEL_PASSIVE, FCTL_GETPANELINFO, 0, &piPasv))
		return;

	if (piActv.PanelType != PTYPE_FILEPANEL || piPasv.PanelType != PTYPE_FILEPANEL || piActv.PluginHandle || piPasv.PluginHandle)
	{
		DisplayMessage(L"Error", L"Only file panels are supported", NULL, true, true);
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
		DisplayMessage(L"Error", L"Can not compare panel to itself", NULL, true, true);
		return;
	}

	if (!AskForCompareParams(cmpAlgo, recursive))
		return;

	FarAdvControl(ACTL_SETPROGRESSSTATE, TBPS_INDETERMINATE, NULL);

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
	char szHashValueActive[128] = {0};
	char szHashValuePassive[128] = {0};
	bool fAborted = false;

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

		if (RunGeneration(strActvPath, cmpAlgo, progressCtx, szHashValueActive, fAborted)
			&& RunGeneration(strPasvPath, cmpAlgo, progressCtx, szHashValuePassive, fAborted))
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

	FarAdvControl(ACTL_SETPROGRESSSTATE, TBPS_NOPROGRESS, NULL);
	FarAdvControl(ACTL_PROGRESSNOTIFY, 0, NULL);

	if (!fAborted)
	{
		DisplayValidationResults(vMismatches, vMissing, nFilesSkipped);
	}
}

//-----------------------------------  Export functions ----------------------------------------

void WINAPI GetGlobalInfoW(struct GlobalInfo *Info)
{
	Info->StructSize=sizeof(GlobalInfo);
	Info->MinFarVersion = FARMANAGERVERSION;
	Info->Version = MAKEFARVERSION(PLUGIN_VERSION_MAJOR, PLUGIN_VERSION_MINOR, PLUGIN_VERSION_REVISION, 0, VS_RELEASE);
	Info->Guid = GUID_PLUGIN_MAIN;
	Info->Title = L"Integrity Checker";
	Info->Description = L"Hash sums generator/verifier plugin";
	Info->Author = L"Ariman";
}

void WINAPI SetStartupInfoW(const struct PluginStartupInfo *Info)
{
	FarSInfo = *Info;
	FSF = *Info->FSF;
	FarSInfo.FSF = &FSF;

	LoadSettings();
}

void WINAPI GetPluginInfoW(struct PluginInfo *Info)
{
	Info->StructSize = sizeof(PluginInfo);
	Info->Flags = 0;

	static const wchar_t *PluginMenuStrings[1];
	PluginMenuStrings[0] = GetLocMsg(MSG_PLUGIN_NAME);

	static const wchar_t *ConfigMenuStrings[1];
	ConfigMenuStrings[0] = GetLocMsg(MSG_PLUGIN_CONFIG_NAME);

	Info->PluginMenu.Guids = &GUID_INFO_MENU;
	Info->PluginMenu.Strings = PluginMenuStrings;
	Info->PluginMenu.Count = sizeof(PluginMenuStrings) / sizeof(PluginMenuStrings[0]);
	Info->PluginConfig.Guids = &GUID_INFO_CONFIG;
	Info->PluginConfig.Strings = ConfigMenuStrings;
	Info->PluginConfig.Count = sizeof(ConfigMenuStrings) / sizeof(ConfigMenuStrings[0]);
	Info->CommandPrefix = optPrefix;
}

void WINAPI ExitFARW(const ExitInfo* Info)
{
	//
}

intptr_t WINAPI ConfigureW(const ConfigureInfo* Info)
{
	LPCWSTR *algoList = new LPCWSTR[NUMBER_OF_SUPPORTED_HASHES];
	int selectedAlgo = 0;
	for (int i = 0; i < NUMBER_OF_SUPPORTED_HASHES; i++)
	{
		algoList[i] = SupportedHashes[i].AlgoName.c_str();
		if (SupportedHashes[i].AlgoId == optDefaultAlgo)
			selectedAlgo = i;
	}

	PluginDialogBuilder dlgBuilder(FarSInfo, GUID_PLUGIN_MAIN, GUID_DIALOG_CONFIG, GetLocMsg(MSG_CONFIG_TITLE), L"IntCheckerConfig");

	dlgBuilder.AddText(MSG_CONFIG_DEFAULT_ALGO);
	dlgBuilder.AddComboBox(&selectedAlgo, NULL, 15, algoList, NUMBER_OF_SUPPORTED_HASHES, DIF_DROPDOWNLIST);

	dlgBuilder.AddSeparator();
	dlgBuilder.AddCheckbox(MSG_CONFIG_PREFIX, (BOOL*) &optUsePrefix);
	
	FarDialogItem* editItem = dlgBuilder.AddEditField(optPrefix, ARRAY_SIZE(optPrefix), 16);
	editItem->X1 += 3;
	
	dlgBuilder.AddCheckbox(MSG_CONFIG_CONFIRM_ABORT, (BOOL*) &optConfirmAbort);
	dlgBuilder.AddCheckbox(MSG_CONFIG_CLEAR_SELECTION, (BOOL*) &optClearSelectionOnComplete);
	dlgBuilder.AddCheckbox(MSG_CONFIG_AUTOEXT, (BOOL*) &optAutoExtension);

	dlgBuilder.AddOKCancel(MSG_BTN_OK, MSG_BTN_CANCEL, -1, true);

	if (dlgBuilder.ShowDialog())
	{
		optDefaultAlgo = SupportedHashes[selectedAlgo].AlgoId;
		
		SaveSettings();
		return TRUE;
	}

	return FALSE;
}

HANDLE WINAPI OpenW(const struct OpenInfo *OInfo)
{
	if ((OInfo->OpenFrom == OPEN_COMMANDLINE) && optUsePrefix)
	{
		OpenCommandLineInfo* cmdInfo = (OpenCommandLineInfo*) OInfo->Data;

		wchar_t* szLocalNameBuffer = _wcsdup(cmdInfo->CommandLine);
		FSF.Unquote(szLocalNameBuffer);
		
		if (!RunValidateFiles(szLocalNameBuffer, true))
			DisplayMessage(GetLocMsg(MSG_DLG_ERROR), GetLocMsg(MSG_DLG_NOTVALIDLIST), NULL, true, true);

		free(szLocalNameBuffer);
	}
	else if (OInfo->OpenFrom == OPEN_PLUGINSMENU)
	{
		PanelInfo pi = {sizeof(PanelInfo), 0};
		if (!FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, &pi) || (pi.PanelType != PTYPE_FILEPANEL))
		{
			return INVALID_HANDLE_VALUE;
		}

		FarMenuItem MenuItems[] = {
			{MIF_NONE, GetLocMsg(MSG_MENU_GENERATE), 0},
			{MIF_NONE, GetLocMsg(MSG_MENU_COMPARE),  0},
			{MIF_NONE, GetLocMsg(MSG_MENU_VALIDATE), 0}
		};

		wstring selectedFilePath;
		int nNumMenuItems = 2;

		if (optDetectHashFiles && (pi.SelectedItemsNumber == 1) && GetSelectedPanelFilePath(selectedFilePath))
		{
			nNumMenuItems = IsFile(selectedFilePath.c_str()) ? 3 : 2;
		}

		intptr_t nMItem = FarSInfo.Menu(&GUID_PLUGIN_MAIN, &GUID_DIALOG_MENU, -1, -1, 0, 0, GetLocMsg(MSG_PLUGIN_NAME), NULL, NULL, NULL, NULL, MenuItems, nNumMenuItems);

		switch (nMItem)
		{
		case 0:
			RunGenerateHashes();
			break;
		case 1:
			RunComparePanels();
			break;
		case 2:
			RunValidateFiles(selectedFilePath.c_str(), false);
			break;
		}
	}

	return INVALID_HANDLE_VALUE;
}
