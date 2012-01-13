// IntChecker2.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "far2/plugin.hpp"
#include "Utils.h"

PluginStartupInfo FarSInfo;
static FARSTANDARDFUNCTIONS FSF;

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
	Info->CommandPrefix = L"check";
}

int WINAPI ConfigureW(int ItemNumber)
{
	//TODO: implement
	return FALSE;
}

HANDLE WINAPI OpenPluginW(int OpenFrom, INT_PTR Item)
{
	// Filter open locations, just in case
	if ((OpenFrom != OPEN_PLUGINSMENU) && (OpenFrom != OPEN_COMMANDLINE))
		return INVALID_HANDLE_VALUE;
	
	PanelInfo PInfo = {0};
	if (!FarSInfo.Control(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, (LONG_PTR) &PInfo))
		return INVALID_HANDLE_VALUE;

	// Only support regular file panels
	if ((PInfo.PanelType != PTYPE_FILEPANEL) || (PInfo.Plugin > 0)) return INVALID_HANDLE_VALUE;
	// Exit if no files present or selected
	if ((PInfo.ItemsNumber == 0) || (PInfo.SelectedItemsNumber == 0)) return INVALID_HANDLE_VALUE;

	if (OpenFrom == OPEN_COMMANDLINE)
	{
		// We are from prefix
	}
	else if (OpenFrom == OPEN_PLUGINSMENU)
	{
		// We are from regular plug-ins menu

		bool fCanComparePanels = ArePanelsComparable();

	} // OpenFrom check
		
	// Update panel
	FarSInfo.Control(PANEL_ACTIVE, FCTL_UPDATEPANEL, 0, NULL);
	FarSInfo.Control(PANEL_ACTIVE, FCTL_REDRAWPANEL, 0, NULL);

	return INVALID_HANDLE_VALUE;
}
