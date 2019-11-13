#include "stdafx.h"

#include <far3/plugin.hpp>
#include <far3/DlgBuilder.hpp>
#include <far3/PluginSettings.hpp>

#include "version.h"
#include "Utils.h"
#include "trust.h"

#include <InitGuid.h>
#include "Far3Guids.h"

#include "FarCommon.h"
#include "farhelpers/Far3Menu.hpp"

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

static bool GetSelectedPanelItemPath(wstring& nameStr)
{
	nameStr.clear();

	PanelInfo pi = {sizeof(PanelInfo)};
	if (FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, &pi))
		if ((pi.SelectedItemsNumber == 1) && (pi.PanelType == PTYPE_FILEPANEL))
		{
			wstring strNameBuffer;
			GetPanelDir(PANEL_ACTIVE, strNameBuffer);
			
			size_t itemBufSize = FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_GETCURRENTPANELITEM, 0, NULL);
			PluginPanelItem *PPI = (PluginPanelItem*)malloc(itemBufSize);
			if (PPI)
			{
				FarGetPluginPanelItem FGPPI={sizeof(FarGetPluginPanelItem), itemBufSize, PPI};
				FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_GETCURRENTPANELITEM, 0, &FGPPI);

				strNameBuffer.append(PPI->FileName);
				nameStr = strNameBuffer;

				free(PPI);
			}
		}

	return (nameStr.size() > 0);
}

static bool PathMatchFileFilter(const PluginPanelItem* item, HANDLE fileFilter)
{
	if (fileFilter != INVALID_HANDLE_VALUE)
	{
		return FarSInfo.FileFilterControl(fileFilter, FFCTL_ISFILEINFILTER, 0, (void *) item) != FALSE;
	}
	return true;
}

static bool CALLBACK FindMatchFileFilter(const WIN32_FIND_DATA* data, HANDLE fileFilter)
{
	if (fileFilter != INVALID_HANDLE_VALUE)
	{
		PluginPanelItem item = {0};
		item.FileName = data->cFileName;
		item.AlternateFileName = data->cAlternateFileName;
		item.FileSize = data->nFileSizeLow + ((unsigned long long)data->nFileSizeHigh << 32);
		item.FileAttributes = data->dwFileAttributes;
		item.CreationTime = data->ftCreationTime;
		item.LastWriteTime = data->ftLastWriteTime;
		item.LastAccessTime = data->ftLastAccessTime;

		return PathMatchFileFilter(&item, fileFilter);
	}
	return true;
}

static void GetSelectedPanelFiles(PanelInfo &pi, wstring &panelDir, StringList &vDest, int64_t &totalSize, bool recursive, HANDLE fileFilter)
{
	
	for (size_t i = 0; i < pi.SelectedItemsNumber; i++)
	{
		size_t requiredBytes = FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_GETSELECTEDPANELITEM, i, NULL);
		PluginPanelItem *PPI = (PluginPanelItem*)malloc(requiredBytes);
		if (PPI)
		{
			FarGetPluginPanelItem FGPPI = {sizeof(FarGetPluginPanelItem), requiredBytes, PPI};
			FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_GETSELECTEDPANELITEM, i, &FGPPI);
			if (wcscmp(PPI->FileName, L"..") && wcscmp(PPI->FileName, L".") && PathMatchFileFilter(PPI, fileFilter))
			{
				if ((PPI->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
				{
					vDest.push_back(PPI->FileName);
					totalSize += PPI->FileSize;
				}
				else
				{
					wstring strSelectedDir = panelDir + PPI->FileName;
					PrepareFilesList(strSelectedDir.c_str(), PPI->FileName, vDest, totalSize, recursive, FindMatchFileFilter, fileFilter);
				}
			}
			free(PPI);
		}
	}
}

static bool GetFarWindowSize(RectSize &size)
{
	SMALL_RECT farRect;
	if (FarSInfo.AdvControl(&GUID_PLUGIN_MAIN, ACTL_GETFARRECT, 0, &farRect))
	{
		size.Assign(farRect);
		return true;
	}

	return false;
}

static const wchar_t* GetMacroStringValue(OpenMacroInfo *macroInfo, size_t index, const wchar_t* defaultValue = L"")
{
	if ((index < macroInfo->Count) && (macroInfo->Values[index].Type == FMVT_STRING))
		return macroInfo->Values[index].String;

	return defaultValue;
}

static bool GetMacroBoolValue(OpenMacroInfo *macroInfo, size_t index, bool defaultValue = false)
{
	if ((index < macroInfo->Count) && (macroInfo->Values[index].Type == FMVT_BOOLEAN))
		return (bool) macroInfo->Values[index].Boolean;

	return defaultValue;
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
	optHashUppercase			= ps.Get(0, L"HashInUppercase", optHashUppercase);
	optRememberLastUsedAlgo		= ps.Get(0, L"RememberLastAlgorithm", optRememberLastUsedAlgo);
	optListDefaultCodepage		= ps.Get(0, L"DefaultListCodepage", optListDefaultCodepage);
	optDefaultOutputTarget		= ps.Get(0, L"DefaultOutput", optDefaultOutputTarget);

	const wchar_t* prefixVal = ps.Get(0, L"Prefix", optPrefix);
	if (prefixVal != optPrefix)
	{
		wcscpy_s(optPrefix, ARRAY_SIZE(optPrefix), prefixVal);
	}
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
	ps.Set(0, L"HashInUppercase", optHashUppercase);
	ps.Set(0, L"RememberLastAlgorithm", optRememberLastUsedAlgo);
	ps.Set(0, L"DefaultListCodepage", optListDefaultCodepage);
	ps.Set(0, L"DefaultOutput", optDefaultOutputTarget);
}

static std::wstring FileSizeToString(int64_t fileSize, bool keepBytes)
{
	wchar_t tmpBuf[64] = { 0 };
	FSF.FormatFileSize(fileSize, 0, keepBytes ? FFFS_COMMAS : FFFS_FLOATSIZE, tmpBuf, _countof(tmpBuf));
	return tmpBuf;
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
		
		static wchar_t szGeneratingLine[128] = {0};
		swprintf_s(szGeneratingLine, ARRAY_SIZE(szGeneratingLine), GetLocMsg(MSG_DLG_CALCULATING), prCtx->HashAlgoName.c_str());

		std::wstring strFilesNumLine = JoinProgressLine(GetLocMsg(MSG_DLG_PROGRESS_NUMFILES), FormatString(L"%d / %d", prCtx->CurrentFileIndex + 1, prCtx->TotalFilesCount), cntProgressDialogWidth, 5);
		std::wstring strBytesLine = JoinProgressLine(GetLocMsg(MSG_DLG_PROGRESS_NUMBYTES), FileSizeToString(prCtx->TotalProcessedBytes, true) + L" / " + FileSizeToString(prCtx->TotalFilesSize, true), cntProgressDialogWidth, 5);
		
		std::wstring strPBarCurrent = ProgressBarString(nFileProgress, cntProgressDialogWidth);
		std::wstring strPBarTotal = ProgressBarString(nTotalProgress, cntProgressDialogWidth);

		int64_t elapsedTime = prCtx->GetElapsedTimeMS();
		int64_t avgSpeed = (elapsedTime > 0) ? (prCtx->TotalProcessedBytes * 1000) / elapsedTime : 0;
		
		std::wstring elapsedTimeStr = GetLocMsg(MSG_DLG_ELAPSEDTIME) + std::wstring(L" ") + DurationToString(elapsedTime);
		std::wstring avgSpeedStr = FileSizeToString(avgSpeed, false) + L"/s";
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

		FarSInfo.Message(&GUID_PLUGIN_MAIN, &GUID_MESSAGE_BOX, 0, NULL, InfoLines, ARRAY_SIZE(InfoLines), 0);

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

static void SelectFilesOnPanel(HANDLE hPanel, std::vector<std::wstring> &fileNames, bool exclusive)
{
	if (!exclusive && (fileNames.size() == 0)) return;

	PanelInfo pi = {sizeof(PanelInfo), 0};
	if (!FarSInfo.PanelControl(hPanel, FCTL_GETPANELINFO, 0, &pi))
		return;

	FarSInfo.PanelControl(hPanel, FCTL_BEGINSELECTION, 0, NULL);

	for (size_t i = 0; i < pi.ItemsNumber; i++)
	{
		size_t nSize = FarSInfo.PanelControl(hPanel, FCTL_GETPANELITEM, i, NULL);
		PluginPanelItem *PPI = (PluginPanelItem*) malloc(nSize);
		if (PPI)
		{
			FarGetPluginPanelItem gppi = {sizeof(FarGetPluginPanelItem), nSize, PPI};
			FarSInfo.PanelControl(hPanel, FCTL_GETPANELITEM, i, &gppi);
			bool isNameInList = std::find(fileNames.begin(), fileNames.end(), PPI->FileName) != fileNames.end();
			if (isNameInList || exclusive)
			{
				FarSInfo.PanelControl(hPanel, FCTL_SETSELECTION, i, (void*)(UINT_PTR)(isNameInList ? TRUE : FALSE));
			}
			free(PPI);
		}
	}

	FarSInfo.PanelControl(hPanel, FCTL_ENDSELECTION, 0, NULL);
	FarSInfo.PanelControl(hPanel, FCTL_REDRAWPANEL, 0, NULL);
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

	return (int)FarSInfo.Message(&GUID_PLUGIN_MAIN, &GUID_MESSAGE_BOX, FMSG_WARNING, NULL, DlgLines, ARRAY_SIZE(DlgLines), 4);
}

static bool RunGeneration(const std::wstring& filePath, const std::wstring& fileDisplayPath, rhash_ids hashAlgo, bool useHashUppercase, ProgressContext& progressCtx, std::string& hashStr, bool &shouldAbort, bool &shouldSkipAllErrors)
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

		//Prepare list
		std::vector<wstring> displayStrings;

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

		RectSize listSize(57, 14);
		FindBestListBoxSize(displayStrings, GetFarWindowSize, listSize);

		// Display dialog

		PluginDialogBuilder dlgBuilder(FarSInfo, GUID_PLUGIN_MAIN, GUID_DIALOG_RESULTS, MSG_DLG_VALIDATION_COMPLETE, nullptr);

		const wchar_t* *boxList = new const wchar_t*[displayStrings.size()];
		for (size_t i = 0; i < displayStrings.size(); i++)
		{
			boxList[i] = displayStrings[i].c_str();
		}

		dlgBuilder.AddListBox(nullptr, listSize.Width, listSize.Height, boxList, displayStrings.size(), DIF_LISTNOBOX | DIF_LISTNOCLOSE);
		dlgBuilder.AddOKCancel(MSG_BTN_CLOSE, MSG_BTN_CLIPBOARD, -1, true);

		intptr_t exitCode = dlgBuilder.ShowDialogEx();
		if (exitCode == 1)
		{
			CopyTextToClipboard(displayStrings);
		}

		delete [] boxList;

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
	int selectedCP = 0;
	for (int i = 0; i < ARRAY_SIZE(codePageValues); i++)
	{
		if (codePageValues[i] == optListDefaultCodepage)
			selectedCP = i;
	}

	PluginDialogBuilder dlgBuilder(FarSInfo, GUID_PLUGIN_MAIN, GUID_DIALOG_PARAMS, MSG_MENU_VALIDATE, L"ValidateParams");

	auto cpBox = dlgBuilder.AddComboBox(&selectedCP, NULL, 8, codePageNames, ARRAY_SIZE(codePageNames), DIF_DROPDOWNLIST);
	dlgBuilder.AddTextBefore(cpBox, MSG_GEN_CODEPAGE);
	dlgBuilder.AddCheckbox(MSG_DLG_IGNORE_MISSING, &ignoreMissingFiles);
	dlgBuilder.AddOKCancel(MSG_BTN_RUN, MSG_BTN_CANCEL);

	if (dlgBuilder.ShowDialog())
	{
		codepage = codePageValues[selectedCP];

		return true;
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

	FarAdvControl(ACTL_SETPROGRESSSTATE, TBPS_INDETERMINATE, NULL);

	// Prepare files list
	{
		FarScreenSave screen;
		DisplayMessage(GetLocMsg(MSG_DLG_PROCESSING), GetLocMsg(MSG_DLG_PREPARE_LIST), NULL, false, false);

		int64_t fileSize;
		for (size_t i = 0; i < hashes.GetCount(); i++)
		{
			const FileHashInfo& fileInfo = hashes.GetFileInfo(i);

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
		ProgressContext progressCtx((int)existingFiles.size(), totalFilesSize);

		bool fAutoSkipErrors = false;
		bool fAborted = false;
		for (size_t i = 0; i < existingFiles.size(); i++)
		{
			const FileHashInfo& fileInfo = hashes.GetFileInfo(existingFiles[i]);

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
		} // for

		if (!fAborted)
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
	const int nTextBoxIndex = 15;
	const int nUseFilterBoxIndex = 19;
	const int nFilterButtonIndex = 24;

	if (Msg == DN_BTNCLICK)
	{
		if (optAutoExtension && Param2 && (Param1 >= 2) && (Param1 <= 2 + NUMBER_OF_SUPPORTED_HASHES))
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
		else if (Param1 == nUseFilterBoxIndex)
		{
			FarSInfo.SendDlgMessage(hDlg, DM_ENABLE, nFilterButtonIndex, (void*)(UINT_PTR)(Param2 ? TRUE : FALSE));
		}
		else if (Param1 == nFilterButtonIndex)
		{
			intptr_t userData = FarSInfo.SendDlgMessage(hDlg, DM_GETITEMDATA, nFilterButtonIndex, nullptr);
			if (userData)
			{
				FarSInfo.FileFilterControl((HANDLE) userData, FFCTL_OPENFILTERSMENU, 0, nullptr);
			}
			return TRUE;
		}
	}

	return FarSInfo.DefDlgProc(hDlg, Msg, Param1, Param2);
}

static bool AskForHashGenerationParams(HashGenerationParams& genParams)
{
	int algoIndex = GetAlgoIndex(genParams.Algorithm);
	int algoNames[] = {MSG_ALGO_CRC, MSG_ALGO_MD5, MSG_ALGO_SHA1, MSG_ALGO_SHA256, MSG_ALGO_SHA512, MSG_ALGO_SHA3_512, MSG_ALGO_WHIRLPOOL};
	int targetIndex = 0;
	int doRecurse = genParams.Recursive;
	int doStoreAbsPath = genParams.StoreAbsPaths;
	
	int useFilter = 0;
	HANDLE hFilter = INVALID_HANDLE_VALUE;
	FarSInfo.FileFilterControl(PANEL_NONE, FFCTL_CREATEFILEFILTER, FFT_CUSTOM, &hFilter);
	
	wchar_t outputFileBuf[MAX_PATH] = {0};
	wcscpy_s(outputFileBuf, ARRAY_SIZE(outputFileBuf), genParams.OutputFileName.c_str());

	const wchar_t* codePageNames[] = {L"UTF-8", L"ANSI", L"OEM"};
	const UINT codePageValues[] = {CP_UTF8, CP_ACP, CP_OEMCP};
	int selectedCP = 0;
	for (int i = 0; i < ARRAY_SIZE(codePageValues); i++)
	{
		if (codePageValues[i] == optListDefaultCodepage)
			selectedCP = i;
	}
		
	PluginDialogBuilder dlgBuilder(FarSInfo, GUID_PLUGIN_MAIN, GUID_DIALOG_PARAMS, MSG_GEN_TITLE, L"GenerateParams", HashParamsDlgProc);

	dlgBuilder.AddText(MSG_GEN_ALGO);
	dlgBuilder.AddRadioButtons(&algoIndex, ARRAY_SIZE(algoNames), algoNames);
	dlgBuilder.AddSeparator();
	dlgBuilder.AddText(MSG_GEN_TARGET);
	
	const int displayTargetsMsgIds[] = {MSG_GEN_TO_FILE, MSG_GEN_TO_SEPARATE, MSG_GEN_TO_DIRS, MSG_GEN_TO_SCREEN};
	const HashOutputTargets displayTargetValues[] = {OT_SINGLEFILE, OT_SEPARATEFILES, OT_SEPARATEDIRS, OT_DISPLAY};
	for (int i = 0; i < _countof(displayTargetValues); ++i)
		if (displayTargetValues[i] == genParams.OutputTarget) targetIndex = i;
	
	auto firstRadio = dlgBuilder.AddRadioButtons(&targetIndex, _countof(displayTargetsMsgIds), displayTargetsMsgIds);
	firstRadio[1].Y1++;
	firstRadio[2].Y1++;
	firstRadio[3].Y1++;
	
	auto editFileName = dlgBuilder.AddEditField(outputFileBuf, MAX_PATH, 30);
	editFileName->Flags = DIF_EDITEXPAND|DIF_EDITPATH|DIF_FOCUS;
	editFileName->X1 += 2;
	editFileName->Y1 -= 3;
	
	dlgBuilder.AddSeparator();
	dlgBuilder.AddCheckbox(MSG_GEN_RECURSE, &doRecurse, 0, false);
	dlgBuilder.AddCheckbox(MSG_GEN_ABSPATH, &doStoreAbsPath);
	dlgBuilder.AddCheckbox(MSG_DLG_USE_FILTER, &useFilter);

	auto cpBox = dlgBuilder.AddComboBox(&selectedCP, NULL, 8, codePageNames, ARRAY_SIZE(codePageNames), DIF_DROPDOWNLIST);
	dlgBuilder.AddTextBefore(cpBox, MSG_GEN_CODEPAGE);

	dlgBuilder.AddSeparator();
	int btnMsgIDs[] = { MSG_BTN_RUN, MSG_BTN_FILTER, MSG_BTN_CANCEL };
	auto firstButtonPtr = dlgBuilder.AddButtons(3, btnMsgIDs, 0, 2);
	firstButtonPtr[1].Flags |= DIF_DISABLE;  // Disable filter button by default
	firstButtonPtr[1].UserData = (intptr_t) hFilter;

	if (dlgBuilder.ShowDialog())
	{
		genParams.Recursive = doRecurse != 0;
		genParams.StoreAbsPaths = doStoreAbsPath != 0;
		genParams.Algorithm = SupportedHashes[algoIndex].AlgoId;
		genParams.OutputTarget = displayTargetValues[targetIndex];
		genParams.OutputFileName = outputFileBuf;
		genParams.OutputFileCodepage = codePageValues[selectedCP];

		if (useFilter)
		{
			genParams.FileFilter = hFilter;
		}
		else
		{
			genParams.FileFilter = INVALID_HANDLE_VALUE;
			FarSInfo.FileFilterControl(hFilter, FFCTL_FREEFILEFILTER, 0, nullptr);
		}
		
		return true;
	}

	FarSInfo.FileFilterControl(hFilter, FFCTL_FREEFILEFILTER, 0, nullptr);
	return false;
}

static void DisplayHashListOnScreen(const HashList &list)
{
	vector<wstring> listStrDump;
	LPCWSTR *listBoxItems = new LPCWSTR[list.GetCount()];

	for (size_t i = 0; i < list.GetCount(); i++)
	{
		listStrDump.push_back(list.GetFileInfo(i).ToString());

		wstring &line = listStrDump[i];
		listBoxItems[i] = line.c_str();
	}

	RectSize listSize(54, 15);
	FindBestListBoxSize(listStrDump, GetFarWindowSize, listSize);
	
	PluginDialogBuilder dlgBuilder(FarSInfo, GUID_PLUGIN_MAIN, GUID_DIALOG_RESULTS, MSG_DLG_CALC_COMPLETE, nullptr);

	dlgBuilder.AddListBox(NULL, listSize.Width, listSize.Height, listBoxItems, list.GetCount(), DIF_LISTNOCLOSE | DIF_LISTNOBOX);
	dlgBuilder.AddOKCancel(MSG_BTN_CLOSE, MSG_BTN_CLIPBOARD, -1, true);
	
	intptr_t exitCode = dlgBuilder.ShowDialogEx();
	if (exitCode == 1)
	{
		CopyTextToClipboard(listStrDump);
	}

	delete [] listBoxItems;
}

static void RunGenerateHashes()
{
	// Check panel for compatibility
	PanelInfo pi = {sizeof(PanelInfo), 0};
	if (!FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, &pi) || (pi.SelectedItemsNumber <= 0))
	{
		DisplayMessage(GetLocMsg(MSG_DLG_ERROR), GetLocMsg(MSG_DLG_NO_FILES_SELECTED), NULL, true, true);
		return;
	}

	// Generation params
	HashGenerationParams genParams;
	std::wstring strOutputFilePath;

	HashAlgoInfo *selectedHashInfo = GetAlgoInfo(genParams.Algorithm);
	if (!selectedHashInfo) return;

	if (optAutoExtension) genParams.OutputFileName += selectedHashInfo->DefaultExt;

	// If only one file is selected then offer it's name as base for hash file name
	if (pi.SelectedItemsNumber == 1)
	{
		wstring strSelectedFile;
		if (GetSelectedPanelItemPath(strSelectedFile) && (strSelectedFile != L".."))
		{
			genParams.OutputFileName = ExtractFileName(strSelectedFile) + selectedHashInfo->DefaultExt;
		}
	}

	while(true)
	{
		if (!AskForHashGenerationParams(genParams))
			return;

		if (genParams.OutputTarget == OT_SINGLEFILE)
		{
			strOutputFilePath = ConvertPathToNative(genParams.OutputFileName);
			
			// Check if hash file already exists
			if (IsFile(strOutputFilePath))
			{
				wchar_t wszMsgText[256] = {0};
				swprintf_s(wszMsgText, ARRAY_SIZE(wszMsgText), GetLocMsg(MSG_DLG_OVERWRITE_FILE_TEXT), genParams.OutputFileName.c_str());

				if (!ConfirmMessage(GetLocMsg(MSG_DLG_OVERWRITE_FILE), wszMsgText, true))
					continue;
			}
			// Check if we can write target file
			else if (!CanCreateFile(strOutputFilePath.c_str()))
			{
				DisplayMessage(MSG_DLG_ERROR, MSG_DLG_CANT_SAVE_HASHLIST, genParams.OutputFileName.c_str(), true, true);
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
	FarAdvControl(ACTL_SETPROGRESSSTATE, TBPS_INDETERMINATE, NULL);

	// Prepare files list
	{
		FarScreenSave screen;
		DisplayMessage(GetLocMsg(MSG_DLG_PROCESSING), GetLocMsg(MSG_DLG_PREPARE_LIST), NULL, false, false);

		GetPanelDir(PANEL_ACTIVE, strPanelDir);
		GetSelectedPanelFiles(pi, strPanelDir, filesToProcess, totalFilesSize, genParams.Recursive, genParams.FileFilter);
	}

	{
		// Perform hashing
		ProgressContext progressCtx((int)filesToProcess.size(), totalFilesSize);
		progressCtx.SetAlgorithm(genParams.Algorithm);

		bool continueSave = true;
		bool fAutoSkipErrors = false;
		for (auto cit = filesToProcess.begin(); cit != filesToProcess.end(); cit++)
		{
			wstring strNextFile = *cit;
			wstring strFullPath = strPanelDir + strNextFile;
			
			std::string hashValueStr;
			bool fShouldAbort = false;

			if (RunGeneration(strFullPath, strNextFile, genParams.Algorithm, optHashUppercase != FALSE, progressCtx, hashValueStr, fShouldAbort, fAutoSkipErrors))
			{
				hashes.SetFileHash(genParams.StoreAbsPaths ? strFullPath : strNextFile, hashValueStr, genParams.Algorithm);
			}
			else if (fShouldAbort)
			{
				continueSave = false;
				break;
			}
		}

		FarAdvControl(ACTL_SETPROGRESSSTATE, TBPS_NOPROGRESS, NULL);
		FarAdvControl(ACTL_PROGRESSNOTIFY, 0, NULL);

		if (genParams.FileFilter != INVALID_HANDLE_VALUE)
			FarSInfo.FileFilterControl(genParams.FileFilter, FFCTL_FREEFILEFILTER, 0, nullptr);

		if (!continueSave) return;
	}

	// Display/save hash list
	bool saveSuccess = false;
	if (genParams.OutputTarget == OT_SINGLEFILE)
	{
		saveSuccess = hashes.SaveList(strOutputFilePath.c_str(), genParams.OutputFileCodepage);
		if (!saveSuccess)
		{
			DisplayMessage(MSG_DLG_ERROR, MSG_DLG_CANT_SAVE_HASHLIST, genParams.OutputFileName.c_str(), true, true);
		}
	}
	else if (genParams.OutputTarget == OT_SEPARATEFILES)
	{
		int numGood, numBad;
		saveSuccess = hashes.SaveListSeparate(strPanelDir.c_str(), genParams.OutputFileCodepage, numGood, numBad);
	}
	else if (genParams.OutputTarget == OT_DISPLAY)
	{
		saveSuccess = true;
		DisplayHashListOnScreen(hashes);
	}
	else if (genParams.OutputTarget == OT_SEPARATEDIRS)
	{
		int numGood, numBad;
		saveSuccess = hashes.SaveListEachDir(strPanelDir.c_str(), genParams.OutputFileCodepage, numGood, numBad);
	}
	else
	{
		// Something went wrong. This should not happen.
		DisplayMessage(L"Error", L"Invalid output target", nullptr, true, true);
	}

	// Clear selection if requested
	if (saveSuccess && optClearSelectionOnComplete)
	{
		for (int i = (int) pi.SelectedItemsNumber - 1; i >=0; i--)
			FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_CLEARSELECTION, i, NULL);
	}

	if (optRememberLastUsedAlgo)
	{
		optDefaultAlgo = genParams.Algorithm;
		SaveSettings();
	}

	FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_REDRAWPANEL, 0, NULL);
}

static intptr_t WINAPI CompareParamsDlgProc(HANDLE hDlg, intptr_t Msg, intptr_t Param1, void* Param2)
{
	const int nUseFilterBoxIndex = 11;
	const int nFilterButtonIndex = 14;

	if (Msg == DN_BTNCLICK)
	{
		if (Param1 == nUseFilterBoxIndex)
		{
			FarSInfo.SendDlgMessage(hDlg, DM_ENABLE, nFilterButtonIndex, (void*)(UINT_PTR)(Param2 ? TRUE : FALSE));
		}
		else if (Param1 == nFilterButtonIndex)
		{
			intptr_t userData = FarSInfo.SendDlgMessage(hDlg, DM_GETITEMDATA, nFilterButtonIndex, nullptr);
			if (userData)
			{
				FarSInfo.FileFilterControl((HANDLE) userData, FFCTL_OPENFILTERSMENU, 0, nullptr);
			}
			return TRUE;
		}
	}

	return FarSInfo.DefDlgProc(hDlg, Msg, Param1, Param2);
}

static bool AskForCompareParams(rhash_ids &selectedAlgo, bool &recursive, HANDLE &fileFilter)
{
	int doRecurse = recursive;
	int algoIndex = GetAlgoIndex(selectedAlgo);
	int algoNames[] = { MSG_ALGO_CRC, MSG_ALGO_MD5, MSG_ALGO_SHA1, MSG_ALGO_SHA256, MSG_ALGO_SHA512, MSG_ALGO_SHA3_512, MSG_ALGO_WHIRLPOOL };

	int useFilter = 0;
	HANDLE hFilter = INVALID_HANDLE_VALUE;
	FarSInfo.FileFilterControl(PANEL_NONE, FFCTL_CREATEFILEFILTER, FFT_CUSTOM, &hFilter);
	
	PluginDialogBuilder dlgBuilder(FarSInfo, GUID_PLUGIN_MAIN, GUID_DIALOG_PARAMS, MSG_DLG_COMPARE, nullptr, CompareParamsDlgProc);

	dlgBuilder.AddText(MSG_GEN_ALGO);
	dlgBuilder.AddRadioButtons(&algoIndex, ARRAY_SIZE(algoNames), algoNames, false);
	dlgBuilder.AddSeparator();
	dlgBuilder.AddCheckbox(MSG_GEN_RECURSE, &doRecurse, 0, false);
	dlgBuilder.AddCheckbox(MSG_DLG_USE_FILTER, &useFilter);
	dlgBuilder.AddSeparator();

	int btnMsgIDs[] = { MSG_BTN_RUN, MSG_BTN_FILTER, MSG_BTN_CANCEL };
	auto firstButtonPtr = dlgBuilder.AddButtons(3, btnMsgIDs, 0, 2);
	firstButtonPtr[1].Flags |= DIF_DISABLE;  // Disable filter button by default
	firstButtonPtr[1].UserData = (intptr_t) hFilter;

	if (dlgBuilder.ShowDialog())
	{
		recursive = doRecurse != 0;
		selectedAlgo = SupportedHashes[algoIndex].AlgoId;

		if (useFilter)
		{
			fileFilter = hFilter;
		}
		else
		{
			fileFilter = INVALID_HANDLE_VALUE;
			FarSInfo.FileFilterControl(hFilter, FFCTL_FREEFILEFILTER, 0, nullptr);
		}
		
		return true;
	}

	return false;
}

static void RunComparePanels()
{
	PanelInfo piActv = {sizeof(PanelInfo), 0}, piPasv = {sizeof(PanelInfo), 0};
	if (!FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, &piActv)
		|| !FarSInfo.PanelControl(PANEL_PASSIVE, FCTL_GETPANELINFO, 0, &piPasv))
		return;

	if (piActv.PanelType != PTYPE_FILEPANEL || piPasv.PanelType != PTYPE_FILEPANEL || piActv.PluginHandle || piPasv.PluginHandle)
	{
		DisplayMessage(GetLocMsg(MSG_DLG_ERROR), GetLocMsg(MSG_DLG_FILE_PANEL_REQUIRED), NULL, true, true);
		return;
	}

	// Nothing selected on the panel
	if (piActv.SelectedItemsNumber == 0) return;

	wstring strActivePanelDir, strPassivePanelDir;
		
	GetPanelDir(PANEL_ACTIVE, strActivePanelDir);
	GetPanelDir(PANEL_PASSIVE, strPassivePanelDir);

	if (strActivePanelDir == strPassivePanelDir)
	{
		DisplayMessage(GetLocMsg(MSG_DLG_ERROR), GetLocMsg(MSG_DLG_NO_COMPARE_SELF), NULL, true, true);
		return;
	}

	rhash_ids cmpAlgo = (rhash_ids) optDefaultAlgo;
	bool recursive = true;
	HANDLE fileFilter = INVALID_HANDLE_VALUE;

	if (!AskForCompareParams(cmpAlgo, recursive, fileFilter))
		return;

	StringList vSelectedFiles;
	int64_t totalFilesSize = 0;

	FarAdvControl(ACTL_SETPROGRESSSTATE, TBPS_INDETERMINATE, NULL);

	// Prepare files list
	{
		FarScreenSave screen;
		DisplayMessage(GetLocMsg(MSG_DLG_PROCESSING), GetLocMsg(MSG_DLG_PREPARE_LIST), NULL, false, false);

		GetSelectedPanelFiles(piActv, strActivePanelDir, vSelectedFiles, totalFilesSize, true, fileFilter);
	}

	if (fileFilter != INVALID_HANDLE_VALUE)
		FarSInfo.FileFilterControl(fileFilter, FFCTL_FREEFILEFILTER, 0, nullptr);

	// No suitable items selected for comparison
	if (vSelectedFiles.size() == 0) return;

	std::vector<std::wstring> vMismatches, vMissing;
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

		FarAdvControl(ACTL_SETPROGRESSSTATE, TBPS_NOPROGRESS, NULL);
		FarAdvControl(ACTL_PROGRESSNOTIFY, 0, NULL);
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

static void RunCompareWithClipboard(const std::wstring &selectedFile)
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
		FarMenu algoMenu(&FarSInfo, &GUID_PLUGIN_MAIN, &GUID_DIALOG_MENU, GetLocMsg(MSG_DLG_SELECT_ALGORITHM));
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

	FarAdvControl(ACTL_SETPROGRESSSTATE, TBPS_NOPROGRESS, NULL);
	FarAdvControl(ACTL_PROGRESSNOTIFY, 0, NULL);
}

static bool SelectBenchmarkParams(int &numBuffers, int *algoList)
{
	PluginDialogBuilder dlgBuilder(FarSInfo, GUID_PLUGIN_MAIN, GUID_DIALOG_PARAMS, MSG_DLG_SELECT_PARAMS, nullptr);
	for (int i = 0; i < NUMBER_OF_SUPPORTED_HASHES; ++i)
	{
		dlgBuilder.AddCheckbox(SupportedHashes[i].AlgoName.c_str(), &algoList[i]);
	}
	dlgBuilder.AddSeparator();
	auto editBox = dlgBuilder.AddIntEditField(&numBuffers, 8);
	dlgBuilder.AddTextBefore(editBox, L"Data Size");
	dlgBuilder.AddTextAfter(editBox, L"Mb");
	dlgBuilder.AddOKCancel(MSG_BTN_OK, MSG_BTN_CANCEL);

	return dlgBuilder.ShowDialog();
}

static void RunBenchmark()
{
	const size_t nBenchBufferSize = 1 * 1024 * 1024; // 1Mb

	int nNumBuffers = 1024;
	int vAlgoList[NUMBER_OF_SUPPORTED_HASHES] = {0};

	std::fill_n(vAlgoList, NUMBER_OF_SUPPORTED_HASHES, 1);
	if (!SelectBenchmarkParams(nNumBuffers, vAlgoList))
		return;

	if ((nNumBuffers <= 0) || std::accumulate(std::begin(vAlgoList), std::end(vAlgoList), 0) <= 0)
	{
		DisplayMessage(MSG_DLG_ERROR, MSG_DLG_INVALID_PARAMS, nullptr, true, true);
		return;
	}

	std::vector<std::wstring> vBenchResults;
	size_t nBenchDataSize = nNumBuffers * nBenchBufferSize;

	FarAdvControl(ACTL_SETPROGRESSSTATE, TBPS_INDETERMINATE, NULL);

	bool fAborted = false;
	for (int i = 0; i < NUMBER_OF_SUPPORTED_HASHES; ++i)
	{
		if (!vAlgoList[i]) continue;

		const HashAlgoInfo& algoInfo = SupportedHashes[i];
		
		std::wstring strBenchingAlgo = FormatString(GetLocMsg(MSG_DLG_BENCHMARKING), algoInfo.AlgoName.c_str());

		FarScreenSave screen;
		DisplayMessage(GetLocMsg(MSG_DLG_PROCESSING), strBenchingAlgo.c_str(), NULL, false, false);

		int64_t benchMs = BenchmarkAlgorithm(algoInfo.AlgoId, nBenchDataSize, nBenchBufferSize);
		if (benchMs < 0)
		{
			fAborted = true;
			break;
		}

		std::wstring strBenchSpeed = FileSizeToString((int64_t)nBenchDataSize * 1000 / benchMs, false) + L"b/s";
		std::wstring strBenchData = FormatString(L"%9s : %llu ms, %s", algoInfo.AlgoName.c_str(), benchMs, strBenchSpeed.c_str());
		vBenchResults.push_back(strBenchData);
	}

	FarAdvControl(ACTL_SETPROGRESSSTATE, TBPS_NOPROGRESS, NULL);

	if (fAborted) return;

	std::wstring strDataSizeInfo = std::wstring(L"Total data size: ") + FileSizeToString(nBenchDataSize, false);

	PluginDialogBuilder dlgBuilder(FarSInfo, GUID_PLUGIN_MAIN, GUID_DIALOG_RESULTS, MSG_DLG_CALC_COMPLETE, nullptr);
	dlgBuilder.AddText(strDataSizeInfo.c_str());
	dlgBuilder.AddSeparator();

	for (size_t i = 0; i < vBenchResults.size(); ++i)
	{
		std::wstring& strResult = vBenchResults[i];
		dlgBuilder.AddText(strResult.c_str());
	}

	dlgBuilder.AddOKCancel(MSG_BTN_OK, -1);
	dlgBuilder.ShowDialog();
}

static void RunVerifySignature(const std::wstring& path)
{
	bool isError = false;
	const wchar_t* messageText = nullptr;
	
	SignedFileInformation fileInfo;
	long errCode = VerifyPeSignature(path.c_str(), fileInfo);
	
	switch (errCode)
	{
	case ERROR_SUCCESS:
		messageText = L"File signature is valid";
		break;
	case TRUST_E_NOSIGNATURE:
		messageText = GetLocMsg(MSG_DLG_NOSIGNATURE);
		break;
	case TRUST_E_EXPLICIT_DISTRUST:
		messageText = L"The certificate was explicitly marked as untrusted by the user.";
		isError = true;
		break;
	case TRUST_E_SUBJECT_NOT_TRUSTED:
		messageText = L"The subject is not trusted for the specified action.";
		isError = true;
		break;
	case CRYPT_E_SECURITY_SETTINGS:
		messageText = L"The cryptographic operation failed due to a local security option setting.";
		isError = true;
		break;
	case CRYPT_E_FILE_ERROR:
		messageText = GetLocMsg(MSG_DLG_FILE_ERROR);
		isError = true;
		break;
	case TRUST_E_PROVIDER_UNKNOWN:
		messageText = L"Unknown trust provider.";
		isError = true;
		break;
	case TRUST_E_BAD_DIGEST:
		messageText = L"The digital signature of the object did not verify.";
		isError = true;
		break;
	case TRUST_E_NO_SIGNER_CERT:
		messageText = L"The certificate for the signer of the message is invalid or not found.";
		isError = true;
		break;
	case CERT_E_UNTRUSTEDROOT:
		messageText = L"Root certificate is not trusted by the trust provider.";
		isError = true;
		break;
	case CERT_E_CHAINING:
		messageText = L"A certificate chain could not be built to a trusted root authority.";
		isError = true;
		break;
	default:
		messageText = L"Something went wrong. Unrecognized response.";
		isError = true;
		break;
	}

	DisplayMessage(GetLocMsg(MSG_DLG_OPERATION_COMPLETE), messageText, nullptr, isError, true);
}

static bool CalculateHashByAlgoName(const wchar_t* algoName, const wchar_t* path, wchar_t* hashBuf, size_t hashBufSize, bool fQuiet, bool &fAborted)
{
	fAborted = false;
	
	int algoIndex = GetAlgoIndexByName(algoName);
	if (algoIndex < 0) return false;

	wstring strFullPath = ConvertPathToNative(path);

	int64_t fileSize = GetFileSize_i64(strFullPath.c_str());
	if (fileSize <= 0) return false;

	rhash_ids algo = SupportedHashes[algoIndex].AlgoId;
	std::string strHashValue;
	bool fSkipAllErrors = fQuiet;

	ProgressContext progressCtx(1, fileSize);

	bool genResult = RunGeneration(strFullPath, path, algo, optHashUppercase != FALSE, progressCtx, strHashValue, fAborted, fSkipAllErrors);
	if (genResult)
	{
		memset(hashBuf, 0, hashBufSize * sizeof(wchar_t));
		MultiByteToWideChar(CP_UTF8, 0, strHashValue.c_str(), -1, hashBuf, (int) hashBufSize);
	}

	FarAdvControl(ACTL_SETPROGRESSSTATE, TBPS_NOPROGRESS, NULL);
	FarAdvControl(ACTL_PROGRESSNOTIFY, 0, NULL);
	
	return genResult;
}

static void WINAPI FreeMacroCall(void *CallbackData, struct FarMacroValue *Values, size_t Count)
{
	FarMacroCall* macroCall = (FarMacroCall*)CallbackData;

	delete macroCall->Values;
	delete macroCall;
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

	const wchar_t* codePageNames[] = {L"UTF-8", L"ANSI", L"OEM"};
	const UINT codePageValues[] = {CP_UTF8, CP_ACP, CP_OEMCP};
	int selectedCP = 0;
	for (int i = 0; i < ARRAY_SIZE(codePageValues); i++)
	{
		if (codePageValues[i] == optListDefaultCodepage)
			selectedCP = i;
	}

	const int outputTargetNames[] = {MSG_CONFIG_OUTPUT_SINGLE_FILE, MSG_CONFIG_OUTPUT_SEPARATE_FILE, MSG_CONFIG_OUTPUT_SEPARATE_DIRS, MSG_CONFIG_OUTPUT_DISPLAY};
	const int outputTargetValues[] = {OT_SINGLEFILE, OT_SEPARATEFILES, OT_SEPARATEDIRS, OT_DISPLAY};
	int selectedOutput = 0;
	for (int i = 0; i < ARRAY_SIZE(outputTargetNames); i++)
	{
		if (outputTargetValues[i] == optDefaultOutputTarget)
			selectedOutput = i;
	}

	PluginDialogBuilder dlgBuilder(FarSInfo, GUID_PLUGIN_MAIN, GUID_DIALOG_CONFIG, GetLocMsg(MSG_CONFIG_TITLE), L"IntCheckerConfig");

	dlgBuilder.AddText(MSG_CONFIG_DEFAULT_ALGO);
	dlgBuilder.AddComboBox(&selectedAlgo, NULL, 15, algoList, NUMBER_OF_SUPPORTED_HASHES, DIF_DROPDOWNLIST);
	dlgBuilder.AddCheckbox(MSG_CONFIG_REMEMBER_LAST_ALGO, (BOOL*) &optRememberLastUsedAlgo);

	dlgBuilder.AddSeparator();
	dlgBuilder.AddText(MSG_CONFIG_DEFAULT_OUTPUT);
	dlgBuilder.AddComboBox(&selectedOutput, NULL, 28, outputTargetNames, ARRAY_SIZE(outputTargetNames), DIF_DROPDOWNLIST);

	dlgBuilder.AddSeparator();
	dlgBuilder.AddCheckbox(MSG_CONFIG_PREFIX, (BOOL*) &optUsePrefix);
	
	FarDialogItem* editItem = dlgBuilder.AddEditField(optPrefix, ARRAY_SIZE(optPrefix), 16);
	editItem->X1 += 3;
	
	dlgBuilder.AddCheckbox(MSG_CONFIG_CONFIRM_ABORT, (BOOL*) &optConfirmAbort);
	dlgBuilder.AddCheckbox(MSG_CONFIG_CLEAR_SELECTION, (BOOL*) &optClearSelectionOnComplete);
	dlgBuilder.AddCheckbox(MSG_CONFIG_AUTOEXT, (BOOL*) &optAutoExtension);
	dlgBuilder.AddCheckbox(MSG_CONFIG_UPPERCASE, (BOOL*) &optHashUppercase);
	auto cpBox = dlgBuilder.AddComboBox(&selectedCP, NULL, 8, codePageNames, ARRAY_SIZE(codePageNames), DIF_DROPDOWNLIST);
	dlgBuilder.AddTextBefore(cpBox, MSG_CONFIG_DEFAULT_CP);

	dlgBuilder.AddOKCancel(MSG_BTN_OK, MSG_BTN_CANCEL, -1, true);

	if (dlgBuilder.ShowDialog())
	{
		optDefaultAlgo = SupportedHashes[selectedAlgo].AlgoId;
		optListDefaultCodepage = codePageValues[selectedCP];
		optDefaultOutputTarget = outputTargetValues[selectedOutput];
		
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

		wchar_t wszLocalNameBuffer[PATH_BUFFER_SIZE] = {0};

		wcscpy_s(wszLocalNameBuffer, ARRAY_SIZE(wszLocalNameBuffer), cmdInfo->CommandLine);
		TrimStr(wszLocalNameBuffer);
		FSF.Unquote(wszLocalNameBuffer);
		FSF.ConvertPath(CPM_FULL, wszLocalNameBuffer, wszLocalNameBuffer, ARRAY_SIZE(wszLocalNameBuffer));
		
		if (!RunValidateFiles(wszLocalNameBuffer, true, false))
			DisplayMessage(GetLocMsg(MSG_DLG_ERROR), GetLocMsg(MSG_DLG_NOTVALIDLIST), NULL, true, true);
	}
	else if (OInfo->OpenFrom == OPEN_PLUGINSMENU)
	{
		PanelInfo pi = {sizeof(PanelInfo), 0};
		if (!FarSInfo.PanelControl(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, &pi) || (pi.PanelType != PTYPE_FILEPANEL) || ((pi.PluginHandle != NULL) && !(pi.Flags & PFLAGS_REALNAMES)))
		{
			return NULL;
		}

		FarMenu openMenu(&FarSInfo, &GUID_PLUGIN_MAIN, &GUID_DIALOG_MENU, GetLocMsg(MSG_PLUGIN_NAME));

		openMenu.AddItemEx(GetLocMsg(MSG_MENU_GENERATE), std::bind(RunGenerateHashes));
		openMenu.AddItemEx(GetLocMsg(MSG_MENU_COMPARE), std::bind(RunComparePanels));

		wstring selectedFilePath;
		if ((pi.SelectedItemsNumber == 1) && GetSelectedPanelItemPath(selectedFilePath) && IsFile(selectedFilePath))
		{
			//TODO: use optDetectHashFiles
			openMenu.AddItemEx(GetLocMsg(MSG_MENU_VALIDATE), std::bind(RunValidateFiles, selectedFilePath.c_str(), false, false));
			openMenu.AddItemEx(GetLocMsg(MSG_MENU_VALIDATE_WITH_PARAMS), std::bind(RunValidateFiles, selectedFilePath.c_str(), false, true));
			openMenu.AddItemEx(GetLocMsg(MSG_MENU_COMPARE_CLIP), std::bind(RunCompareWithClipboard, selectedFilePath));

			if (FileCanHaveSignature(selectedFilePath.c_str()))
			{
				openMenu.AddSeparator();
				openMenu.AddItemEx(GetLocMsg(MSG_MENU_SIGNATURE), std::bind(RunVerifySignature, selectedFilePath));
			}
		}

		openMenu.AddSeparator();
		openMenu.AddItemEx(GetLocMsg(MSG_MENU_BENCHMARK), std::bind(RunBenchmark));

		openMenu.RunEx();
	}
	else if (OInfo->OpenFrom == OPEN_FROMMACRO)
	{
		OpenMacroInfo *macroInfo = (OpenMacroInfo*)OInfo->Data;

		if (macroInfo->StructSize != sizeof(OpenMacroInfo))
			return 0;
		if (macroInfo->Count < 3)
			return 0;

		const wchar_t* strOp = GetMacroStringValue(macroInfo, 0);
		const wchar_t* strAlgoName = GetMacroStringValue(macroInfo, 1);
		const wchar_t* strTarget = GetMacroStringValue(macroInfo, 2);
		bool fQuiet = GetMacroBoolValue(macroInfo, 3, false);

		if (SameText(strOp, L"gethash"))
		{
			static wchar_t hashValue[256] = { 0 };
			bool fCalcAborted = false;
			
			if (CalculateHashByAlgoName(strAlgoName, strTarget, hashValue, _countof(hashValue), fQuiet, fCalcAborted) || fCalcAborted)
			{
				FarMacroValue *macroVal = new FarMacroValue();
				macroVal->Type = FMVT_STRING;
				macroVal->String = fCalcAborted ? L"userabort" : hashValue;

				FarMacroCall *macroCall = new FarMacroCall();
				macroCall->StructSize = sizeof(FarMacroCall);
				macroCall->Count = 1;
				macroCall->Values = macroVal;
				macroCall->Callback = FreeMacroCall;
				macroCall->CallbackData = macroCall;

				return macroCall;
			}
		}
	}

	return NULL;
}
