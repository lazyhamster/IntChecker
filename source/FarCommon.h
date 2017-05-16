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

const size_t cntProgressDialogWidth = 65;

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
		TotalProcessedBytes(0), CurrentFileProcessedBytes(0), FileProgress(-1), TotalProgress(-1),
		m_nPrevTotalBytes(0), m_nPrevTotalProgress(-1)
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
		m_nPrevTotalProgress = TotalProgress;
		
		m_sFilePath = displayPath;
		m_sShortenedFilePath = ShortenPath(displayPath, cntProgressDialogWidth);
		CurrentFileIndex++;
		CurrentFileProcessedBytes = 0;
		CurrentFileSize = fileSize;
		FileProgress = -1;
	}

	void NextFile(const std::wstring& displayPath)
	{
		NextFile(displayPath, GetFileSize_i64(displayPath.c_str()));
	}

	void RestartFile()
	{
		TotalProcessedBytes = m_nPrevTotalBytes;
		TotalProgress = m_nPrevTotalProgress;
		FileProgress = -1;
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

		int nFileProgress = (CurrentFileSize > 0) ? (int)((CurrentFileProcessedBytes * 100) / CurrentFileSize) : 0;
		int nTotalProgress = (TotalFilesSize > 0) ? (int)((TotalProcessedBytes * 100) / TotalFilesSize) : 0;
		
		if (nFileProgress != FileProgress || nTotalProgress != TotalProgress)
		{
			FileProgress = nFileProgress;
			TotalProgress = nTotalProgress;

			return true;
		}

		return false;
	}

	const wchar_t* GetShortenedPath() const
	{
		return m_sShortenedFilePath.c_str();
	}

	int GetCurrentFileProgress() const
	{
		return FileProgress;
	}

	int GetTotalProgress() const
	{
		return TotalProgress;
	}

private:
	std::wstring m_sFilePath;
	std::wstring m_sShortenedFilePath;

	int64_t m_nPrevTotalBytes;
	int m_nPrevTotalProgress;

	// This is percentage cache to prevent screen flicker
	int FileProgress;
	int TotalProgress;

	ProgressContext() {}
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

#endif // FarCommon_h__
