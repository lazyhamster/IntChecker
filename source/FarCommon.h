#ifndef FarCommon_h__
#define FarCommon_h__

#include "hashing.h"
#include "Lang.h"

PluginStartupInfo FarSInfo;
static FARSTANDARDFUNCTIONS FSF;

enum HashOutputTargets
{
	OT_SINGLEFILE = 0,		// One single hash file for all selected input files
	OT_SEPARATEFILES = 1,	// One hash file per one input file
	OT_DISPLAY = 2,
	OT_SEPARATEDIRS = 3		// One hash file per one directory
};

// Plugin settings
static bool optDetectHashFiles = true;
static bool optClearSelectionOnComplete = true;
static bool optConfirmAbort = true;
static bool optAutoExtension = true;
static int optDefaultAlgo = RHASH_MD5;
static int optDefaultOutputTarget = OT_SINGLEFILE;
static bool optUsePrefix = true;
static bool optHashUppercase = false;
static int optListDefaultCodepage = CP_UTF8;
static bool optRememberLastUsedAlgo = false;
static wchar_t optPrefix[32] = L"check";

#define EDR_SKIP 0
#define EDR_SKIPALL 1
#define EDR_RETRY 2
#define EDR_ABORT 3

const size_t cntProgressDialogWidth = 65;
const int cntProgressRedrawTimeout = 50; // ms

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

static std::wstring ShortenPath(const std::wstring &path, size_t maxWidth)
{
	if (path.length() > maxWidth)
	{
		wchar_t* tmpBuf = _wcsdup(path.c_str());
		FSF.TruncPathStr(tmpBuf, maxWidth);

		std::wstring result(tmpBuf);
		free(tmpBuf);
		return result;
	}

	return path;
}

struct ProgressContext
{
	ProgressContext(int totalFiles, int64_t totalBytes)
		: TotalFilesSize(totalBytes), TotalFilesCount(totalFiles), CurrentFileSize(0), CurrentFileIndex(-1),
		TotalProcessedBytes(0), CurrentFileProcessedBytes(0),m_nPrevTotalBytes(0),
		m_tRefreshTime(time_check::mode::immediate, std::chrono::milliseconds(cntProgressRedrawTimeout)), m_tStartTime(clock_type::now())
	{}

	std::wstring HashAlgoName;

	int64_t TotalFilesSize;
	int TotalFilesCount;

	int64_t CurrentFileSize;
	int CurrentFileIndex;

	int64_t TotalProcessedBytes;	
	int64_t CurrentFileProcessedBytes;

	void NextFile(const std::wstring& displayPath, int64_t fileSize)
	{
		m_nPrevTotalBytes = TotalProcessedBytes;
		
		m_sFilePath = displayPath;
		m_sShortenedFilePath = ShortenPath(displayPath, cntProgressDialogWidth);
		CurrentFileIndex++;
		CurrentFileProcessedBytes = 0;
		CurrentFileSize = fileSize;
	}

	void NextFile(const std::wstring& displayPath)
	{
		NextFile(displayPath, GetFileSize_i64(displayPath.c_str()));
	}

	void RestartFile()
	{
		TotalProcessedBytes = m_nPrevTotalBytes;
		CurrentFileProcessedBytes = 0;
	}

	void SetAlgorithm(rhash_ids algo)
	{
		HashAlgoName = GetAlgoInfo(algo)->AlgoName;
	}

	// Returns true if progress dialog needs to be updated
	bool IncreaseProcessedBytes(int64_t bytesProcessed)
	{
		CurrentFileProcessedBytes += bytesProcessed;
		TotalProcessedBytes += bytesProcessed;

		if (!m_tRefreshTime)
			return false;

		return true;
	}

	const wchar_t* GetShortenedPath() const
	{
		return m_sShortenedFilePath.c_str();
	}

	int GetCurrentFileProgress() const
	{
		return (CurrentFileSize > 0) ? (int)((CurrentFileProcessedBytes * 100) / CurrentFileSize) : 0;
	}

	int GetTotalProgress() const
	{
		return (TotalFilesSize > 0) ? (int)((TotalProcessedBytes * 100) / TotalFilesSize) : 0;
	}

	int64_t GetElapsedTimeMS()
	{
		clock_type::duration timeDiff = clock_type::now() - m_tStartTime;
		return std::chrono::duration_cast<std::chrono::milliseconds>(timeDiff).count();
	}

private:
	std::wstring m_sFilePath;
	std::wstring m_sShortenedFilePath;

	int64_t m_nPrevTotalBytes;

	time_check m_tRefreshTime;
	clock_type::time_point m_tStartTime;
	FarScreenSave m_pScreen;

	ProgressContext() = delete;
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

static std::wstring ProgressBarString(intptr_t Percentage, intptr_t Width)
{
	std::wstring result;
	// 0xB0 - 0x2591
	// 0xDB - 0x2588
	result = std::wstring((Width - 5) * (Percentage > 100 ? 100 : Percentage) / 100, 0x2588);
	result += std::wstring((Width - 5) - result.length(), 0x2591);
	result += FormatString(L"%4d%%", Percentage > 100 ? 100 : Percentage);
	return result;
}

static std::wstring DurationToString(int64_t durationMs)
{
	int64_t durationSec = durationMs / 1000;
	
	int64_t hours = durationSec / 3600;
	int64_t mins = (durationSec / 60) - (hours * 60);
	int64_t secs = durationSec - (hours * 3600) - (mins * 60);

	return FormatString(L"%02lld:%02lld:%02lld", hours, mins, secs);
}

static std::wstring JoinProgressLine(const std::wstring &prefix, const std::wstring &suffix, size_t maxWidth, size_t rightPadding)
{
	size_t middlePadding = maxWidth - prefix.length() - suffix.length() - rightPadding;
	std::wstring result = prefix + std::wstring(middlePadding, ' ') + suffix + std::wstring(rightPadding, ' ');

	return result;
}

static std::wstring ConvertPathToNative(const std::wstring &path)
{
	std::vector<wchar_t> Buffer(MAX_PATH);

	for (;;)
	{
		size_t ActualSize = FSF.ConvertPath(CPM_NATIVE, path.c_str(), Buffer.data(), Buffer.size());
		if (ActualSize <= Buffer.size())
			break;
		Buffer.resize(ActualSize);
	}

	return Buffer.data();
}

#endif // FarCommon_h__
