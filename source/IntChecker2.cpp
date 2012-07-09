// IntChecker2.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "far2/plugin.hpp"
#include "Utils.h"
#include "hashing.h"

PluginStartupInfo FarSInfo;
static FARSTANDARDFUNCTIONS FSF;

// Plugin settings
static int optDetectHashFiles = 1;
static int optClearSelectionOnComplete = 1;
static int optConfirmAbort = 1;
static int optDefaultAlgo = RHASH_MD5;
static wchar_t optPrefix[32] = L"check";

static rhash_ids s_SupportedAlgos[] = {RHASH_CRC32, RHASH_MD5, RHASH_SHA1, RHASH_SHA256, RHASH_SHA512, RHASH_WHIRLPOOL};

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
	//
}

static void SaveSettings()
{
	//
}

// Returns true if file is recognized as hash list
static bool RunValidateFiles(const wchar_t* hashListPath, bool silent)
{
	return false;
}

static void RunGenerateHashes()
{
	//TODO: implement
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
