#include "stdafx.h"
#include "far3/plugin.hpp"

// --------------------------------------- Local functions ---------------------------------------------------

static void LoadSettings()
{
	//TODO: implement
}

static void SaveSettings()
{
	//TODO: implement
}

//-----------------------------------  Export functions ----------------------------------------

void WINAPI GetGlobalInfoW(struct GlobalInfo *Info)
{
	Info->StructSize=sizeof(GlobalInfo);
	Info->MinFarVersion = FARMANAGERVERSION;
	Info->Version = MAKEFARVERSION(OBSERVER_VERSION_MAJOR, OBSERVER_VERSION_MINOR, OBSERVER_VERSION_REVISION, 0, VS_RELEASE);
	Info->Guid = OBSERVER_GUID;
	Info->Title = L"Observer";
	Info->Description = L"Container Extractor";
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

	Info->PluginMenu.Guids = &GUID_OBS_INFO_MENU;
	Info->PluginMenu.Strings = PluginMenuStrings;
	Info->PluginMenu.Count = sizeof(PluginMenuStrings) / sizeof(PluginMenuStrings[0]);
	Info->PluginConfig.Guids = &GUID_OBS_INFO_CONFIG;
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
	//TODO: implement
}

HANDLE WINAPI OpenW(const struct OpenInfo *OInfo)
{
	if ((OInfo->OpenFrom == OPEN_COMMANDLINE) && optUsePrefix)
	{
		OpenCommandLineInfo* cmdInfo = (OpenCommandLineInfo*) OInfo->Data;

		//TODO: implement
	}
	else if (OInfo->OpenFrom == OPEN_PLUGINSMENU)
	{
		//TODO: implement
	}

	return INVALID_HANDLE_VALUE;
}
