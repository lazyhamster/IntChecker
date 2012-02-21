// IntChecker2.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "far2/plugin.hpp"
#include "Utils.h"
#include "rhash/librhash/rhash.h"

PluginStartupInfo FarSInfo;
static FARSTANDARDFUNCTIONS FSF;

// Plugin settings
static int optDetectHashFiles = 1;
static int optClearSelectionOnComplete = 1;
static int optConfirmAbort = 1;
static int optDefaultAlgo = RHASH_MD5;
static wchar_t optPrefix[32] = L"check";

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
	}
	else if (OpenFrom == OPEN_PLUGINSMENU)
	{
		// We are from regular plug-ins menu
	} // OpenFrom check
		
	return INVALID_HANDLE_VALUE;
}
