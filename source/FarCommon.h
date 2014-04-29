#ifndef FarCommon_h__
#define FarCommon_h__

#include "hashing.h"
#include "Lang.h"

PluginStartupInfo FarSInfo;
static FARSTANDARDFUNCTIONS FSF;

// Plugin settings
static bool optDetectHashFiles = true;
static bool optClearSelectionOnComplete = true;
static bool optConfirmAbort = true;
static bool optAutoExtension = true;
static int optDefaultAlgo = RHASH_MD5;
static bool optUsePrefix = true;
static wchar_t optPrefix[32] = L"check";

static rhash_ids s_SupportedAlgos[] = {RHASH_CRC32, RHASH_MD5, RHASH_SHA1, RHASH_SHA256, RHASH_SHA512, RHASH_WHIRLPOOL};

enum HashOutputTargets
{
	OT_SINGLEFILE,
	OT_SEPARATEFILES,
	OT_DISPLAY
};

#define EDR_SKIP 0
#define EDR_RETRY 1
#define EDR_ABORT 2

struct ProgressContext
{
	ProgressContext()
		: TotalFilesSize(0), TotalFilesCount(0), CurrentFileSize(0), CurrentFileIndex(-1),
		TotalProcessedBytes(0), CurrentFileProcessedBytes(0), FileProgress(0), TotalProgress(0)
	{}

	wstring FileName;

	int64_t TotalFilesSize;
	int TotalFilesCount;

	int64_t CurrentFileSize;
	int CurrentFileIndex;

	int64_t TotalProcessedBytes;	
	int64_t CurrentFileProcessedBytes;

	// This is percentage cache to prevent screen flicker
	int FileProgress;
	int TotalProgress;
};

class FarScreenSave
{
private:
	HANDLE hScreen;

public:
	FarScreenSave()
	{
		hScreen = FarSInfo.SaveScreen(0, 0, -1, -1);
	}

	~FarScreenSave()
	{
		FarSInfo.RestoreScreen(hScreen);
	}
};

#endif // FarCommon_h__
