#ifndef FarCommon_h__
#define FarCommon_h__

#include "hashing.h"
#include "Lang.h"

PluginStartupInfo FarSInfo;
static FARSTANDARDFUNCTIONS FSF;

enum HashOutputTargets
{
	OT_SINGLEFILE = 0,
	OT_SEPARATEFILES = 1,
	OT_DISPLAY = 2
};

// Plugin settings
static int optDetectHashFiles = TRUE;
static int optClearSelectionOnComplete = TRUE;
static int optConfirmAbort = TRUE;
static int optAutoExtension = TRUE;
static int optDefaultAlgo = RHASH_MD5;
static int optDefaultOutputTarget = OT_SINGLEFILE;
static int optUsePrefix = TRUE;
static int optHashUppercase = FALSE;
static int optListDefaultCodepage = CP_UTF8;
static int optRememberLastUsedAlgo = FALSE;
static wchar_t optPrefix[32] = L"check";

#define EDR_SKIP 0
#define EDR_SKIPALL 1
#define EDR_RETRY 2
#define EDR_ABORT 3

struct ProgressContext
{
	ProgressContext()
		: TotalFilesSize(0), TotalFilesCount(0), CurrentFileSize(0), CurrentFileIndex(-1),
		TotalProcessedBytes(0), CurrentFileProcessedBytes(0), FileProgress(0), TotalProgress(0)
	{}

	std::wstring FileName;
	std::wstring HashAlgoName;

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

struct RectSize
{
	int Width;
	int Height;

	RectSize() : Width(0), Height(0) {}
	RectSize(int aWidth, int aHeight) : Width(aWidth), Height(aHeight) {}

	void Assign(SMALL_RECT sr)
	{
		Width = sr.Right - sr.Left + 1;
		Height = sr.Bottom - sr.Top + 1;
	}
};

struct HashGenerationParams
{
	rhash_ids Algorithm;
	bool Recursive;
	HashOutputTargets OutputTarget;
	bool StoreAbsPaths;
	HANDLE FileFilter;

	std::wstring OutputFileName;
	UINT OutputFileCodepage;
	
	HashGenerationParams()
	{
		Algorithm = (rhash_ids) optDefaultAlgo;
		Recursive = true;
		OutputTarget = (HashOutputTargets) optDefaultOutputTarget;
		StoreAbsPaths = false;
		FileFilter = INVALID_HANDLE_VALUE;
		
		OutputFileName = L"hashlist";
		OutputFileCodepage = optListDefaultCodepage;
	}
};

typedef bool (*FARSIZECALLBACK)(RectSize &farSize);

static bool FindBestListBoxSize(std::vector<std::wstring> listItems, FARSIZECALLBACK sizeFunc, RectSize &listBoxSize)
{
	int maxLineWidth = 0;
	RectSize farSize;

	if (!sizeFunc(farSize))
		return false;

	for (auto cit = listItems.cbegin(); cit != listItems.cend(); cit++)
	{
		const std::wstring &str = *cit;
		maxLineWidth = max(maxLineWidth, (int) str.length());
	}

	maxLineWidth += 2; // spaces on both sides of the text
	if (maxLineWidth > listBoxSize.Width)
		listBoxSize.Width = min(maxLineWidth, farSize.Width - 20);

	int numLines = (int) listItems.size();
	if (numLines > listBoxSize.Height)
		listBoxSize.Height = min(numLines, farSize.Height - 12);

	return true;
}

#endif // FarCommon_h__
